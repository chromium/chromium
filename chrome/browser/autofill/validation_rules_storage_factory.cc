// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/validation_rules_storage_factory.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "third_party/libaddressinput/chromium/chrome_storage_impl.h"

namespace autofill {

using ::i18n::addressinput::Storage;

// static
std::unique_ptr<Storage> ValidationRulesStorageFactory::CreateStorage() {
  // It's OK to leak the ValidationRulesStorageFactory instance; the
  // JsonPrefStore will block on any write during shutdown anyway.
  static base::LazyInstance<ValidationRulesStorageFactory>::Leaky instance =
      LAZY_INSTANCE_INITIALIZER;
  return std::unique_ptr<Storage>(
      new ChromeStorageImpl(instance.Get().json_pref_store_.get()));
}

ValidationRulesStorageFactory::ValidationRulesStorageFactory() {
  base::FilePath user_data_dir;
  bool success = base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(success);

  json_pref_store_ = new JsonPrefStore(
      user_data_dir.Append(FILE_PATH_LITERAL("Address Validation Rules")));
  json_pref_store_->ReadPrefsAsync(nullptr);
}

ValidationRulesStorageFactory::~ValidationRulesStorageFactory() {}

}  // namespace autofill
