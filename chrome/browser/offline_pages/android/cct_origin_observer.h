// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_ANDROID_CCT_ORIGIN_OBSERVER_H_
#define CHROME_BROWSER_OFFLINE_PAGES_ANDROID_CCT_ORIGIN_OBSERVER_H_

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "components/offline_pages/core/offline_page_model.h"

namespace offline_pages {

/**
 * Bridge between C++ and Java for exposing when CCT pages are changed.
 */
class CctOriginObserver : public OfflinePageModel::Observer,
                          public base::SupportsUserData::Data {
 public:
  static void AttachToOfflinePageModel(OfflinePageModel* model);
  CctOriginObserver();
  ~CctOriginObserver() override;

  // OfflinePageModel::Observer implementation.
  void OfflinePageModelLoaded(OfflinePageModel* model) override;
  void OfflinePageAdded(OfflinePageModel* model,
                        const OfflinePageItem& added_page) override;
  void OfflinePageDeleted(const OfflinePageItem& item) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CctOriginObserver);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_ANDROID_CCT_ORIGIN_OBSERVER_H_
