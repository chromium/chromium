// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_TOP_SITE_PROVIDER_BRIDGE_H_
#define CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_TOP_SITE_PROVIDER_BRIDGE_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/ntp_tile.h"

// AuxiliarySearchTopSiteProviderBridge is responsible for providing top sites
// information for the auxiliary search.
class AuxiliarySearchTopSiteProviderBridge
    : public ntp_tiles::MostVisitedSites::Observer {
 public:
  explicit AuxiliarySearchTopSiteProviderBridge(
      std::unique_ptr<ntp_tiles::MostVisitedSites>);

  AuxiliarySearchTopSiteProviderBridge(
      const AuxiliarySearchTopSiteProviderBridge&) = delete;
  AuxiliarySearchTopSiteProviderBridge& operator=(
      const AuxiliarySearchTopSiteProviderBridge&) = delete;

  ~AuxiliarySearchTopSiteProviderBridge() override;

  // Sets an observer and immediately fetches the current most visited sites
  // suggestions.
  void SetObserverAndTrigger(JNIEnv* env,
                             const base::android::JavaRef<jobject>& j_ref_obj);

  // Removes the observer and destroys the bridge.
  void Destroy(JNIEnv* env);

  // Starts a fetch of the current most visited sites suggestions.
  void GetMostVisitedSites(JNIEnv* env) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(AuxiliarySearchTopSiteProviderBridgeTest,
                           AddAndRemoveObservers);

  void RemoveObserver();

  // ntp_tiles::MostVisitedSites::Observer implementation.
  void OnURLsAvailable(
      const std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector>&
          sections) override;
  void OnIconMadeAvailable(const GURL& site_url) override;

  jni_zero::ScopedJavaGlobalRef<jobject> observer_;

  std::unique_ptr<ntp_tiles::MostVisitedSites> most_visited_sites_;
};

#endif  // CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_TOP_SITE_PROVIDER_BRIDGE_H_
