// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_backend.h"

#include "components/password_manager/core/browser/login_database.h"

namespace password_manager {

std::unique_ptr<PasswordStoreBackend> PasswordStoreBackend::Create(
    std::unique_ptr<LoginDatabase> login_db) {
  // TODO(crbug.com/1217071): Once PasswordStoreImpl does not implement the
  // PasswordStore abstract class anymore, return a local backend.
  return nullptr;
}

}  // namespace password_manager
