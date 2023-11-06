// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_registry_update.h"

namespace webapk {
RegistryUpdateData::RegistryUpdateData() = default;
RegistryUpdateData::~RegistryUpdateData() = default;

bool RegistryUpdateData::isEmpty() const {
  return apps_to_create.empty() && apps_to_delete.empty();
}

}  // namespace webapk
