// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_utils.h"

#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_interface.h"

password_manager::PasswordStoreInterface* GetPasswordStore(
    Profile* profile,
    bool use_account_store) {
  if (use_account_store) {
    return AccountPasswordStoreFactory::GetForProfile(
               profile, ServiceAccessType::EXPLICIT_ACCESS)
        .get();
  }
  return PasswordStoreFactory::GetForProfile(profile,
                                             ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}
