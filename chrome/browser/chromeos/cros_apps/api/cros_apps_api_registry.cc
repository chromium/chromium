// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_registry.h"

#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_mutable_registry.h"

const CrosAppsApiRegistry& CrosAppsApiRegistry::GetInstance(Profile* profile) {
  return CrosAppsApiMutableRegistry::GetInstance(profile);
}
