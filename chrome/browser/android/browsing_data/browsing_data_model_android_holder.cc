// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/browsing_data/browsing_data_model_android_holder.h"

namespace {

const void* const kBrowsingDataModelAndroidKey = &kBrowsingDataModelAndroidKey;

}  // namespace

BrowsingDataModelAndroidHolder::BrowsingDataModelAndroidHolder(
    BrowsingDataModelAndroid* model) {
  CHECK(model != nullptr);
  model_weak_ptr_ = model->GetWeakPtr();
}

BrowsingDataModelAndroidHolder::~BrowsingDataModelAndroidHolder() = default;

// static
const void* BrowsingDataModelAndroidHolder::UserDataKey() {
  return kBrowsingDataModelAndroidKey;
}
