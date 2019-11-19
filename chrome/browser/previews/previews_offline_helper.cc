// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_offline_helper.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/offline_pages/core/page_criteria.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"

namespace {
// Pref key for the available hashed pages kept in class.
const char kHashedAvailablePages[] = "previews.offline_helper.available_pages";

void RecordShouldAttemptOfflinePreviewResult(bool result) {
  UMA_HISTOGRAM_BOOLEAN("Previews.Offline.FalsePositivePrevention.Allowed",
                        result);
}

std::string HashURL(const GURL& url) {
  // We are ok with some hash collisions in exchange for non-arbitrary key
  // lengths (as in using the url.spec()). Therefore, use a hash and return that
  // as a string since base::DictionaryValue only accepts strings as keys.
  std::string clean_url = url.GetAsReferrer().spec();
  uint32_t hash = base::PersistentHash(clean_url);
  return base::StringPrintf("%x", hash);
}

std::string TimeToDictionaryValue(base::Time time) {
  return base::NumberToString(time.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

base::Optional<base::Time> TimeFromDictionaryValue(std::string value) {
  int64_t int_value = 0;
  if (!base::StringToInt64(value, &int_value))
    return base::nullopt;

  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(int_value));
}

// Cleans up the given dictionary by removing all stale (expiry has passed)
// entries.
void RemoveStaleOfflinePageEntries(base::DictionaryValue* dict) {
  base::Time earliest_expiry = base::Time::Max();
  std::string earliest_key;
  std::vector<std::string> keys_to_delete;
  for (const auto& iter : dict->DictItems()) {
    // Check for a corrupted value and throw it out if so.
    if (!iter.second.is_string()) {
      keys_to_delete.push_back(iter.first);
      continue;
    }

    base::Optional<base::Time> time =
        TimeFromDictionaryValue(iter.second.GetString());
    if (!time.has_value()) {
      keys_to_delete.push_back(iter.first);
      continue;
    }

    base::Time expiry =
        time.value() + previews::params::OfflinePreviewFreshnessDuration();
    bool is_expired = expiry <= base::Time::Now();

    if (is_expired) {
      keys_to_delete.push_back(iter.first);
      continue;
    }

    if (expiry < earliest_expiry) {
      earliest_key = iter.first;
      earliest_expiry = expiry;
    }
  }

  for (const std::string& key : keys_to_delete)
    dict->RemoveKey(key);

  // RemoveStaleOfflinePageEntries is called for every new added page, so it's
  // fine to just remove one at a time to keep the pref size below a threshold.
  if (dict->DictSize() > previews::params::OfflinePreviewsHelperMaxPrefSize()) {
    dict->RemoveKey(earliest_key);
  }
}

void AddSingleOfflineItemEntry(
    base::DictionaryValue* available_pages,
    const offline_pages::OfflinePageItem& added_page) {
  available_pages->SetKey(
      HashURL(added_page.url),
      base::Value(TimeToDictionaryValue(added_page.creation_time)));

  // Also remember the original url (pre-redirects) if one exists.
  if (!added_page.original_url_if_different.is_empty()) {
    available_pages->SetKey(
        HashURL(added_page.original_url_if_different),
        base::Value(TimeToDictionaryValue(added_page.creation_time)));
  }
}

}  // namespace

PreviewsOfflineHelper::PreviewsOfflineHelper(
    content::BrowserContext* browser_context)
    : pref_service_(nullptr),
      available_pages_(std::make_unique<base::DictionaryValue>()),
      offline_page_model_(nullptr) {
  if (!browser_context || browser_context->IsOffTheRecord())
    return;

  pref_service_ = Profile::FromBrowserContext(browser_context)->GetPrefs();

  available_pages_ =
      pref_service_->GetDictionary(kHashedAvailablePages)->CreateDeepCopy();

  // Tidy up the pref in case it's been a while since the last stale item
  // removal.
  RemoveStaleOfflinePageEntries(available_pages_.get());
  UpdatePref();

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_page_model_ =
      offline_pages::OfflinePageModelFactory::GetForBrowserContext(
          browser_context);

  if (offline_page_model_ &&
      base::FeatureList::IsEnabled(
          previews::features::kOfflinePreviewsFalsePositivePrevention)) {
    offline_page_model_->AddObserver(this);
    // Schedule a low priority task with a slight delay to ensure that the
    // expensive DB query doesn't occur during startup or during other user
    // visible actions.
    base::PostDelayedTask(
        FROM_HERE,
        {base::MayBlock(), content::BrowserThread::UI,
         base::TaskPriority::LOWEST,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&PreviewsOfflineHelper::RequestDBUpdate,
                       weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(30));
  }
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)
}

PreviewsOfflineHelper::~PreviewsOfflineHelper() {
  if (offline_page_model_)
    offline_page_model_->RemoveObserver(this);
}

// static
void PreviewsOfflineHelper::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kHashedAvailablePages);
}

bool PreviewsOfflineHelper::ShouldAttemptOfflinePreview(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!base::FeatureList::IsEnabled(
          previews::features::kOfflinePreviewsFalsePositivePrevention)) {
    // This is the default behavior without this optimization.
    return true;
  }
  std::string hashed_url = HashURL(url);

  base::Value* value = available_pages_->FindKey(hashed_url);
  if (!value) {
    RecordShouldAttemptOfflinePreviewResult(false);
    return false;
  }

  if (!value->is_string()) {
    NOTREACHED();
    RecordShouldAttemptOfflinePreviewResult(false);
    return false;
  }
  base::Optional<base::Time> time_value =
      TimeFromDictionaryValue(value->GetString());
  if (!time_value.has_value()) {
    RecordShouldAttemptOfflinePreviewResult(false);
    return false;
  }

  base::Time expiry =
      time_value.value() + previews::params::OfflinePreviewFreshnessDuration();
  bool is_expired = expiry <= base::Time::Now();
  if (is_expired) {
    available_pages_->RemoveKey(hashed_url);
    UpdatePref();
  }

  RecordShouldAttemptOfflinePreviewResult(!is_expired);
  return !is_expired;
}

void PreviewsOfflineHelper::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (offline_page_model_) {
    offline_page_model_->RemoveObserver(this);
    offline_page_model_ = nullptr;
  }
}

void PreviewsOfflineHelper::RequestDBUpdate() {
  offline_pages::PageCriteria criteria;
  criteria.maximum_matches =
      previews::params::OfflinePreviewsHelperMaxPrefSize();

  offline_page_model_->GetPagesWithCriteria(
      criteria, base::BindOnce(&PreviewsOfflineHelper::UpdateAllPrefEntries,
                               weak_factory_.GetWeakPtr()));
}

void PreviewsOfflineHelper::UpdateAllPrefEntries(
    const offline_pages::MultipleOfflinePageItemResult& pages) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Totally reset the pref with the given vector. We presume that the given
  // |pages| are a full result from a Offline DB query which we take as the
  // source of truth.
  available_pages_->Clear();
  for (const offline_pages::OfflinePageItem& page : pages)
    AddSingleOfflineItemEntry(available_pages_.get(), page);
  RemoveStaleOfflinePageEntries(available_pages_.get());
  UpdatePref();

  UMA_HISTOGRAM_COUNTS_100("Previews.Offline.FalsePositivePrevention.PrefSize",
                           available_pages_->size());
}

void PreviewsOfflineHelper::OfflinePageModelLoaded(
    offline_pages::OfflinePageModel* model) {
  // Ignored.
}

void PreviewsOfflineHelper::OfflinePageAdded(
    offline_pages::OfflinePageModel* model,
    const offline_pages::OfflinePageItem& added_page) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddSingleOfflineItemEntry(available_pages_.get(), added_page);
  RemoveStaleOfflinePageEntries(available_pages_.get());
  UpdatePref();
}

void PreviewsOfflineHelper::OfflinePageDeleted(
    const offline_pages::OfflinePageItem& deleted_page) {
  // Do nothing. OfflinePageModel calls |OfflinePageDeleted| when pages are
  // refreshed, but because we only key on URL and not the offline page id, it
  // is difficult to tell when this happens. So instead, it's ok if we
  // over-trigger for a few pages until the next DB query.
}

void PreviewsOfflineHelper::UpdatePref() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (pref_service_)
    pref_service_->Set(kHashedAvailablePages, *available_pages_);
}
