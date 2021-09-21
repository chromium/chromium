// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_backend.h"

#include "base/feature_list.h"
#include "chrome/browser/password_manager/android/password_store_android_backend.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_bridge.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_store_impl.h"
#include "components/password_manager/core/browser/password_store_proxy_backend.h"
#include "components/password_manager/core/common/password_manager_features.h"

namespace password_manager {

std::unique_ptr<PasswordStoreBackend> PasswordStoreBackend::Create(
    std::unique_ptr<LoginDatabase> login_db) {
  if (base::FeatureList::IsEnabled(
          password_manager::features::kUnifiedPasswordManagerAndroid)) {
    return std::make_unique<PasswordStoreAndroidBackend>(
        PasswordStoreAndroidBackendBridge::Create());
  }
  if (base::FeatureList::IsEnabled(
          password_manager::features::kUnifiedPasswordManagerShadowAndroid)) {
    return std::make_unique<PasswordStoreProxyBackend>(
        std::make_unique<PasswordStoreImpl>(std::move(login_db)),
        std::make_unique<PasswordStoreAndroidBackend>(
            PasswordStoreAndroidBackendBridge::Create()));
  }
  return std::make_unique<PasswordStoreImpl>(std::move(login_db));
}

}  // namespace password_manager
