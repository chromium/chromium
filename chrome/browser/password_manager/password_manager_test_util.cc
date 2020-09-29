// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_test_util.h"

#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"

using password_manager::TestPasswordStore;

scoped_refptr<TestPasswordStore> CreateAndUseTestPasswordStore(
    Profile* profile) {
  TestPasswordStore* store = static_cast<TestPasswordStore*>(
      PasswordStoreFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              profile,
              base::BindRepeating(&password_manager::BuildPasswordStore<
                                  content::BrowserContext, TestPasswordStore>))
          .get());
  return base::WrapRefCounted(store);
}
