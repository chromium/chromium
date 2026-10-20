// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "auxiliary_search_top_site_provider_bridge.h"

#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/flat_set.h"
#include "base/hash/hash.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/auxiliary_search/auxiliary_search_provider.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/ntp_tiles/chrome_most_visited_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/top_sites.h"
#include "components/ntp_tiles/constants.h"
#include "third_party/jni_zero/default_conversions.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"
#include "url/url_constants.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/auxiliary_search/jni_headers/AuxiliarySearchTopSiteProviderBridge_jni.h"

namespace {
// Must match Java Tab.INVALID_TAB_ID.
static constexpr int kInvalidTabId = -1;

constexpr int kMaxNumMostVisitedSites = 4;
constexpr int kMaxScore = 100;

// Converts the index to be an integer score.
int convertSiteSuggestionScore(int index) {
  return kMaxScore - index;
}

// TODO(crbug.com/575576501): Update TileSource enum to include prepopulated
// sites. This way, we don't have to query the TopSitesFactory to filter out
// the prepopulated pages.
// Builds the set of prepopulated page URLs for the user, which are then used
// to filter them out from the most visited sites, e.g. the Chrome Web Store.
// Prepopulated pages are only seeded on Desktop, and on Desktop Android for 3P
// DSEs.
base::flat_set<GURL> GetPrepopulatedPageUrls(Profile* profile) {
  std::vector<GURL> urls;
  scoped_refptr<history::TopSites> top_sites =
      TopSitesFactory::GetForProfile(profile);
  if (top_sites) {
    for (const history::PrepopulatedPage& page :
         top_sites->GetPrepopulatedPages()) {
      urls.push_back(page.most_visited.url);
    }
  }
  return base::flat_set<GURL>(std::move(urls));
}

}  // namespace

AuxiliarySearchTopSiteProviderBridge::AuxiliarySearchTopSiteProviderBridge(
    std::unique_ptr<ntp_tiles::MostVisitedSites> most_visited_sites,
    base::flat_set<GURL> excluded_urls)
    : most_visited_sites_(std::move(most_visited_sites)),
      excluded_urls_(std::move(excluded_urls)) {}

AuxiliarySearchTopSiteProviderBridge::~AuxiliarySearchTopSiteProviderBridge() =
    default;

void AuxiliarySearchTopSiteProviderBridge::SetObserverAndTrigger(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_ref_obj) {
  observer_ = jni_zero::ScopedJavaGlobalRef<jobject>(j_ref_obj);

  CHECK(most_visited_sites_);
  most_visited_sites_->AddMostVisitedURLsObserver(this,
                                                  kMaxNumMostVisitedSites);
}

void AuxiliarySearchTopSiteProviderBridge::Destroy(JNIEnv* env) {
  RemoveObserver();
  delete this;
}

void AuxiliarySearchTopSiteProviderBridge::GetMostVisitedSites(
    JNIEnv* env) const {
  CHECK(most_visited_sites_);

  most_visited_sites_->RefreshTiles();
}

void AuxiliarySearchTopSiteProviderBridge::RemoveObserver() {
  most_visited_sites_->RemoveMostVisitedURLsObserver(this);
  observer_ = nullptr;
}

void AuxiliarySearchTopSiteProviderBridge::OnURLsAvailable(
    bool is_user_triggered,
    const std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector>&
        sections) {
  CHECK(most_visited_sites_);

  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<jni_zero::ScopedJavaLocalRef<jobject>> entries;
  // Uses only personalized tiles for auxiliary search.
  auto it = sections.find(ntp_tiles::SectionType::PERSONALIZED);
  if (it == sections.end()) {
    return;
  }

  int index = 0;
  for (const ntp_tiles::NTPTile& tile : it->second) {
    // Filters the tile list to include only TOP_SITES and CUSTOM_LINKS tiles,
    // and skips excluded URLs.
    if ((tile.source != ntp_tiles::TileSource::TOP_SITES &&
         tile.source != ntp_tiles::TileSource::CUSTOM_LINKS) ||
        excluded_urls_.contains(tile.url)) {
      continue;
    }

    entries.push_back(Java_AuxiliarySearchTopSiteProviderBridge_addDataEntry(
        env, static_cast<int>(AuxiliarySearchEntryType::kTopSite), tile.url,
        tile.title, tile.last_visit_time.InMillisecondsSinceUnixEpoch(),
        kInvalidTabId,
        /*appId=*/std::string(),
        std::abs(static_cast<int>(
            base::Hash(tile.url.spec() + base::UTF16ToUTF8(tile.title)))),
        convertSiteSuggestionScore(index++)));
  }

  Java_AuxiliarySearchTopSiteProviderBridge_onMostVisitedSitesURLsAvailable(
      env, observer_, entries);
}

void AuxiliarySearchTopSiteProviderBridge::OnIconMadeAvailable(
    const GURL& site_url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AuxiliarySearchTopSiteProviderBridge_onIconMadeAvailable(env, observer_,
                                                                site_url);
}

static int64_t JNI_AuxiliarySearchTopSiteProviderBridge_Init(JNIEnv* env,
                                                             Profile* profile) {
  CHECK(profile, base::NotFatalUntil::M161);

  return reinterpret_cast<intptr_t>(new AuxiliarySearchTopSiteProviderBridge(
      ChromeMostVisitedSitesFactory::NewForProfile(profile),
      GetPrepopulatedPageUrls(profile)));
}

DEFINE_JNI(AuxiliarySearchTopSiteProviderBridge)
