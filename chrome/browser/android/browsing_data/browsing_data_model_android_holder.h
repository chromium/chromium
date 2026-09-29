// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_ANDROID_BROWSING_DATA_BROWSING_DATA_MODEL_ANDROID_HOLDER_H_
#define CHROME_BROWSER_ANDROID_BROWSING_DATA_BROWSING_DATA_MODEL_ANDROID_HOLDER_H_

#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "components/browsing_data/content/android/browsing_data_model_android.h"

// Holder class to store a non-owning weak reference to a
// BrowsingDataModelAndroid in Profile user data.
class BrowsingDataModelAndroidHolder : public base::SupportsUserData::Data {
 public:
  // Constructor now takes a raw pointer to the model.
  explicit BrowsingDataModelAndroidHolder(BrowsingDataModelAndroid* model);
  ~BrowsingDataModelAndroidHolder() override;

  BrowsingDataModelAndroidHolder(const BrowsingDataModelAndroidHolder&) =
      delete;
  BrowsingDataModelAndroidHolder& operator=(
      const BrowsingDataModelAndroidHolder&) = delete;

  // Returns a WeakPtr to the model.
  BrowsingDataModelAndroid* model() const { return model_weak_ptr_.get(); }

  static const void* UserDataKey();

 private:
  base::WeakPtr<BrowsingDataModelAndroid> model_weak_ptr_;
};

#endif  // CHROME_BROWSER_ANDROID_BROWSING_DATA_BROWSING_DATA_MODEL_ANDROID_HOLDER_H_
