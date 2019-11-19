// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_BUILTIN_SERVICE_MANIFESTS_H_
#define CHROME_APP_BUILTIN_SERVICE_MANIFESTS_H_

#include <vector>

#include "services/service_manager/public/cpp/manifest.h"

// Returns manifests for all shared (i.e. cross-profile) in-process and
// out-of-process services built into Chrome but not into Content.
const std::vector<service_manager::Manifest>&
GetChromeBuiltinServiceManifests();

#endif  // CHROME_APP_BUILTIN_SERVICE_MANIFESTS_H_
