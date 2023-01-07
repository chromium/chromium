// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_TEST_HELPER_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_TEST_HELPER_H_

#include <memory>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "components/account_id/account_id.h"

namespace apps {
class AppRegistryCache;
}

namespace desks_storage {
class DeskModel;
}

namespace ash {

// This class creates a desk model and has functionality used by unit tests that
// involve saved desk functionality.
class SavedDeskTestHelper {
 public:
  // Creates a desk model. Will CHECK-fail on errors.
  SavedDeskTestHelper();
  SavedDeskTestHelper(const SavedDeskTestHelper&) = delete;
  SavedDeskTestHelper& operator=(const SavedDeskTestHelper&) = delete;
  ~SavedDeskTestHelper();

  // Adds `app_id` to `registry_cache_` for `account_id_`.
  void AddAppIdToAppRegistryCache(const std::string& app_id);

  // Waits for the desk model to become ready. This should be done by any test
  // that actively needs to interact with the desk model. Note: the reason why
  // this isn't done automatically for all tests is that the act of waiting will
  // pump the event loop and a lot of tests apparently cannot handle that.
  // See https://crbug.com/1360638 for more info.
  void WaitForDeskModel();

  desks_storage::DeskModel* desk_model() { return desk_model_.get(); }

 private:
  AccountId account_id_;

  base::ScopedTempDir desk_model_data_dir_;

  std::unique_ptr<desks_storage::DeskModel> desk_model_;

  std::unique_ptr<apps::AppRegistryCache> cache_;
};

}  // namespace ash

#endif
