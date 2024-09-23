// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_MOCK_PASSWORD_STORE_ANDROID_BACKEND_BRIDGE_HELPER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_MOCK_PASSWORD_STORE_ANDROID_BACKEND_BRIDGE_HELPER_H_

#include <jni.h>

#include "chrome/browser/password_manager/android/password_store_android_backend_bridge_helper.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockPasswordStoreAndroidBackendBridgeHelper
    : public password_manager::PasswordStoreAndroidBackendBridgeHelper {
 public:
  MockPasswordStoreAndroidBackendBridgeHelper();
  ~MockPasswordStoreAndroidBackendBridgeHelper() override;

  MOCK_METHOD(bool, CanUseGetAffiliatedPasswordsAPI, (), (override));
  MOCK_METHOD(bool, CanUseGetAllLoginsWithBrandingInfoAPI, (), (override));
  MOCK_METHOD(void, SetConsumer, (base::WeakPtr<Consumer>), (override));
  MOCK_METHOD(JobId, GetAllLogins, (std::string), (override));
  MOCK_METHOD(JobId, GetAllLoginsWithBrandingInfo, (std::string), (override));
  MOCK_METHOD(JobId, GetAutofillableLogins, (std::string), (override));
  MOCK_METHOD(JobId,
              GetLoginsForSignonRealm,
              (const std::string&, std::string),
              (override));
  MOCK_METHOD(JobId,
              AddLogin,
              (const password_manager::PasswordForm&, std::string),
              (override));
  MOCK_METHOD(JobId,
              UpdateLogin,
              (const password_manager::PasswordForm&, std::string),
              (override));
  MOCK_METHOD(JobId,
              RemoveLogin,
              (const password_manager::PasswordForm&, std::string),
              (override));
  MOCK_METHOD(JobId,
              GetAffiliatedLoginsForSignonRealm,
              (const std::string&, std::string),
              (override));
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_MOCK_PASSWORD_STORE_ANDROID_BACKEND_BRIDGE_HELPER_H_
