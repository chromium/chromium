// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/builtin_service_manifests.h"

#include "base/no_destructor.h"
#include "build/build_config.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/manifest.h"
#endif

const std::vector<service_manager::Manifest>&
GetChromeBuiltinServiceManifests() {
  static base::NoDestructor<std::vector<service_manager::Manifest>> manifests{{
#if defined(OS_CHROMEOS)
      ash::GetManifest(),
#endif
  }};
  return *manifests;
}
