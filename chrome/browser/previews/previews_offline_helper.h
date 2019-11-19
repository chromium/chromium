// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_OFFLINE_HELPER_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_OFFLINE_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "url/gurl.h"

class PrefService;

namespace content {
class BrowserContext;
}  // namespace content

class PrefRegistrySimple;
class PrefService;

// This class keeps track of available offline pages to help optimize triggering
// of offline previews for cases that have a high probability of actually having
// an offline page to load.
class PreviewsOfflineHelper : public offline_pages::OfflinePageModel::Observer {
 public:
  explicit PreviewsOfflineHelper(content::BrowserContext* browser_context);
  ~PreviewsOfflineHelper() override;

  // Registers the prefs used in this class.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns true if there is a high likelihood that a offline page exists with
  // the given URL.
  bool ShouldAttemptOfflinePreview(const GURL& url);

  // Removes |this| as an observer from offline pages.
  void Shutdown();

  // Updates all entries in the pref with the given result from an offline page
  // database query.
  void UpdateAllPrefEntries(
      const offline_pages::MultipleOfflinePageItemResult& pages);

  // offline_pages::OfflinePageModel::Observer:
  void OfflinePageModelLoaded(offline_pages::OfflinePageModel* model) override;
  void OfflinePageAdded(
      offline_pages::OfflinePageModel* model,
      const offline_pages::OfflinePageItem& added_page) override;
  void OfflinePageDeleted(
      const offline_pages::OfflinePageItem& deleted_page) override;

  void SetPrefServiceForTesting(PrefService* pref_service) {
    pref_service_ = pref_service;
  }

 private:
  // Requests all eligible pages from Offline Page Model. This is defined as a
  // separate method so that it can be scheduled at a low priority since the
  // Offline DB query is expensive, and only needs to be done at most once per
  // session.
  void RequestDBUpdate();

  // Helper method to update |available_pages_| in |pref_service_|.
  void UpdatePref();

  // A reference to the profile's |PrefService|.
  PrefService* pref_service_;

  // A mapping from url_hash to original storage time (as a double).
  std::unique_ptr<base::DictionaryValue> available_pages_;

  offline_pages::OfflinePageModel* offline_page_model_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PreviewsOfflineHelper> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PreviewsOfflineHelper);
};

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_OFFLINE_HELPER_H_
