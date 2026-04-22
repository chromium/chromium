// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_DATA_MANAGER_ANDROID_TEST_API_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_DATA_MANAGER_ANDROID_TEST_API_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "chrome/browser/autofill/android/entity_data_manager_android.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"

namespace autofill {

// Exposes some testing operations for EntityDataManagerAndroid.
class EntityDataManagerAndroidTestApi {
 public:
  explicit EntityDataManagerAndroidTestApi(
      EntityDataManagerAndroid& entity_data_manager_android)
      : entity_data_manager_android_(entity_data_manager_android) {}

  void AddOrUpdateEntityInstance(
      EntityInstance entity_instance,
      EntityInstance::RecordType targeted_record_type,
      int description_string_id = 0,
      int accept_button_string_id = 0,
      base::OnceClosure on_local_save_fallback = base::DoNothing()) {
    entity_data_manager_android_->AddOrUpdateEntityInstance(
        std::move(entity_instance), targeted_record_type, description_string_id,
        accept_button_string_id, std::move(on_local_save_fallback));
  }

 private:
  const raw_ref<EntityDataManagerAndroid> entity_data_manager_android_;
};

inline EntityDataManagerAndroidTestApi test_api(
    EntityDataManagerAndroid& entity_data_manager_android) {
  return EntityDataManagerAndroidTestApi(entity_data_manager_android);
}

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_DATA_MANAGER_ANDROID_TEST_API_H_
