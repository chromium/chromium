// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/navigation_entry_remover.h"

#include <functional>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/common/buildflags.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#endif  // BUILDFLAG(ENABLE_SESSION_SERVICE)

namespace {

bool ShouldDeleteUrl(base::Time begin,
                     base::Time end,
                     const std::optional<std::set<GURL>>& restrict_urls,
                     const GURL& url,
                     base::Time time_stamp) {
  return begin <= time_stamp && (time_stamp < end || end.is_null()) &&
         (!restrict_urls.has_value() ||
          restrict_urls->find(url) != restrict_urls->end());
}

bool ShouldDeleteNavigationEntry(
    base::Time begin,
    base::Time end,
    const std::optional<std::set<GURL>>& restrict_urls,
    content::NavigationEntry* entry) {
  return ShouldDeleteUrl(begin, end, restrict_urls, entry->GetURL(),
                         entry->GetTimestamp());
}

bool ShouldDeleteSerializedNavigationEntry(
    base::Time begin,
    base::Time end,
    const std::optional<std::set<GURL>>& restrict_urls,
    const sessions::SerializedNavigationEntry& entry) {
  return ShouldDeleteUrl(begin, end, restrict_urls, entry.virtual_url(),
                         entry.timestamp());
}

bool UrlMatcherForNavigationEntry(const base::flat_set<GURL>& urls,
                                  content::NavigationEntry* entry) {
  return urls.find(entry->GetURL()) != urls.end();
}

bool UrlMatcherForSerializedNavigationEntry(
    const base::flat_set<GURL>& urls,
    const sessions::SerializedNavigationEntry& entry) {
  return urls.find(entry.virtual_url()) != urls.end();
}

base::flat_set<GURL> CreateUrlSet(const history::URLRows& deleted_rows) {
  return base::MakeFlatSet<GURL>(deleted_rows, {}, &history::URLRow::url);
}

void DeleteNavigationEntries(
    content::WebContents* web_contents,
    const content::NavigationController::DeletionPredicate& predicate) {
  content::NavigationController* controller = &web_contents->GetController();
  controller->DiscardNonCommittedEntries();
  // We discarded pending and transient entries but there could still be
  // no last_committed_entry, which would prevent deletion.
  if (controller->CanPruneAllButLastCommitted())
    controller->DeleteNavigationEntries(predicate);
}

void DeleteTabNavigationEntries(
    Profile* profile,
    const history::DeletionTimeRange& time_range,
    const std::optional<std::set<GURL>>& restrict_urls,
    const base::flat_set<GURL>& url_set) {
  auto predicate = time_range.IsValid()
                       ? base::BindRepeating(
                             &ShouldDeleteNavigationEntry, time_range.begin(),
                             time_range.end(), std::cref(restrict_urls))
                       : base::BindRepeating(&UrlMatcherForNavigationEntry,
                                             std::cref(url_set));

#if BUILDFLAG(IS_ANDROID)
  auto session_predicate =
      time_range.IsValid()
          ? base::BindRepeating(&ShouldDeleteSerializedNavigationEntry,
                                time_range.begin(), time_range.end(),
                                std::cref(restrict_urls))
          : base::BindRepeating(&UrlMatcherForSerializedNavigationEntry,
                                std::cref(url_set));

  for (const TabModel* tab_model : TabModelList::models()) {
    if (tab_model->GetProfile() == profile) {
      for (int i = 0; i < tab_model->GetTabCount(); i++) {
        TabAndroid* tab = tab_model->GetTabAt(i);
        tab->DeleteFrozenNavigationEntries(session_predicate);
        content::WebContents* web_contents = tab->web_contents();
        if (web_contents)
          DeleteNavigationEntries(web_contents, predicate);
      }
    }
  }
#else
  for (Browser* browser : *BrowserList::GetInstance()) {
    TabStripModel* tab_strip = browser->tab_strip_model();
    if (browser->profile() == profile) {
      for (int i = 0; i < tab_strip->count(); i++)
        DeleteNavigationEntries(tab_strip->GetWebContentsAt(i), predicate);
    }
  }
#endif
}

void PerformTabRestoreDeletion(
    sessions::TabRestoreService* service,
    const sessions::TabRestoreService::DeletionPredicate& predicate) {
  service->DeleteNavigationEntries(predicate);
  service->DeleteLastSession();
}

// This class waits until TabRestoreService is loaded, then deletes
// navigation entries using |predicate| and deletes itself afterwards.
class TabRestoreDeletionHelper : public sessions::TabRestoreServiceObserver {
 public:
  TabRestoreDeletionHelper(
      sessions::TabRestoreService* service,
      const sessions::TabRestoreService::DeletionPredicate& predicate)
      : service_(service), deletion_predicate_(predicate) {
    DCHECK(!service->IsLoaded());
    service->AddObserver(this);
    service->LoadTabsFromLastSession();
  }

  TabRestoreDeletionHelper(const TabRestoreDeletionHelper&) = delete;
  TabRestoreDeletionHelper& operator=(const TabRestoreDeletionHelper&) = delete;

  // sessions::TabRestoreServiceObserver:
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override {
    delete this;
  }

  void TabRestoreServiceLoaded(sessions::TabRestoreService* service) override {
    PerformTabRestoreDeletion(service, deletion_predicate_);
    delete this;
  }

 private:
  ~TabRestoreDeletionHelper() override { service_->RemoveObserver(this); }

  raw_ptr<sessions::TabRestoreService> service_;
  sessions::TabRestoreService::DeletionPredicate deletion_predicate_;
};

void DeleteTabRestoreEntries(Profile* profile,
                             const history::DeletionTimeRange& time_range,
                             const std::optional<std::set<GURL>>& restrict_urls,
                             const base::flat_set<GURL>& url_set) {
  sessions::TabRestoreService* tab_service =
      TabRestoreServiceFactory::GetForProfile(profile);
  if (!tab_service)
    return;

  auto predicate =
      time_range.IsValid()
          ? base::BindRepeating(&ShouldDeleteSerializedNavigationEntry,
                                time_range.begin(), time_range.end(),
                                restrict_urls)
          : base::BindRepeating(&UrlMatcherForSerializedNavigationEntry,
                                url_set);
  if (tab_service->IsLoaded()) {
    PerformTabRestoreDeletion(tab_service, predicate);
  } else {
    // The helper deletes itself when the tab entry deletion is finished.
    new TabRestoreDeletionHelper(tab_service, predicate);
  }
}

void DeleteLastSessionFromSessionService(Profile* profile) {
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile);
  if (session_service)
    session_service->DeleteLastSession();
#endif
}

}  // namespace

namespace browsing_data {

void RemoveNavigationEntries(Profile* profile,
                             const history::DeletionInfo& deletion_info) {
  DCHECK(!profile->IsOffTheRecord());
  DCHECK(!deletion_info.is_from_expiration());

  base::flat_set<GURL> url_set;
  if (!deletion_info.time_range().IsValid())
    url_set = CreateUrlSet(deletion_info.deleted_rows());

  DeleteTabNavigationEntries(profile, deletion_info.time_range(),
                             deletion_info.restrict_urls(), url_set);
  DeleteTabRestoreEntries(profile, deletion_info.time_range(),
                          deletion_info.restrict_urls(), url_set);

  // Removal of navigation entries may occur at any point during runtime and
  // session service data is cleared so that it can be later rebuilt without the
  // deleted entries.
  // However deletion of foreign visits specifically can occur during startup
  // and clearing session service data will delete the user's previous session
  // with no ability to rebuild/recover (see crbug.com/1424800). Foreign visits
  // can't be part of the local session so there is no risk of retaining the
  // session service data in this case.
  if (deletion_info.deletion_reason() !=
      history::DeletionInfo::Reason::kDeleteAllForeignVisits) {
    DeleteLastSessionFromSessionService(profile);
  }
}

}  // namespace browsing_data
