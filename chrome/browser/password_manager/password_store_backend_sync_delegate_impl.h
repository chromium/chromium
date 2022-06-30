// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_BACKEND_SYNC_DELEGATE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_BACKEND_SYNC_DELEGATE_IMPL_H_

#include "components/password_manager/core/browser/password_store_backend.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

class PasswordStoreBackendSyncDelegateImpl
    : public password_manager::PasswordStoreBackend::SyncDelegate {
 public:
  explicit PasswordStoreBackendSyncDelegateImpl(Profile* profile);
  ~PasswordStoreBackendSyncDelegateImpl() override;

 private:
  // SyncDelegate implementation.
  bool IsSyncingPasswordsEnabled() override;
  absl::optional<std::string> GetSyncingAccount() override;

  base::raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_BACKEND_SYNC_DELEGATE_IMPL_H_
