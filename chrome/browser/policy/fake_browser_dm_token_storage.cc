// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/fake_browser_dm_token_storage.h"

namespace policy {

FakeBrowserDMTokenStorage::FakeBrowserDMTokenStorage() {
  BrowserDMTokenStorage::SetForTesting(this);
}

FakeBrowserDMTokenStorage::FakeBrowserDMTokenStorage(
    const std::string& client_id,
    const std::string& enrollment_token,
    const std::string& dm_token,
    bool enrollment_error_option)
    : client_id_(client_id),
      enrollment_token_(enrollment_token),
      dm_token_(dm_token),
      enrollment_error_option_(enrollment_error_option) {}

FakeBrowserDMTokenStorage::~FakeBrowserDMTokenStorage() = default;

void FakeBrowserDMTokenStorage::SetClientId(const std::string& client_id) {
  client_id_ = client_id;
}

void FakeBrowserDMTokenStorage::SetEnrollmentToken(
    const std::string& enrollment_token) {
  enrollment_token_ = enrollment_token;
}

void FakeBrowserDMTokenStorage::SetDMToken(const std::string& dm_token) {
  dm_token_ = dm_token;
}

void FakeBrowserDMTokenStorage::SetEnrollmentErrorOption(bool option) {
  enrollment_error_option_ = option;
}

void FakeBrowserDMTokenStorage::EnableStorage(bool storage_enabled) {
  storage_enabled_ = storage_enabled;
}

std::string FakeBrowserDMTokenStorage::InitClientId() {
  return client_id_;
}

std::string FakeBrowserDMTokenStorage::InitEnrollmentToken() {
  return enrollment_token_;
}

std::string FakeBrowserDMTokenStorage::InitDMToken() {
  return dm_token_;
}

bool FakeBrowserDMTokenStorage::InitEnrollmentErrorOption() {
  return enrollment_error_option_;
}

void FakeBrowserDMTokenStorage::SaveDMToken(const std::string& token) {
  OnDMTokenStored(storage_enabled_);
}

}  // namespace policy
