// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/chromeos/chromeos_utils.h"

#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/parent_access_ash.h"
#include "chromeos/crosapi/mojom/parent_access.mojom.h"

namespace supervised_user {

crosapi::mojom::ParentAccess* GetParentAccessApi() {
  return crosapi::CrosapiManager::Get()->crosapi_ash()->parent_access_ash();
}

}  // namespace supervised_user
