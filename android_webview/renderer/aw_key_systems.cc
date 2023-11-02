// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_key_systems.h"
#include "components/cdm/renderer/android_key_systems.h"

namespace android_webview {

void AwAddKeySystems(std::vector<std::unique_ptr<media::KeySystemInfo>>*
                         key_systems_properties) {
#if BUILDFLAG(ENABLE_WIDEVINE)
  cdm::AddAndroidWidevine(key_systems_properties);
#endif  // BUILDFLAG(ENABLE_WIDEVINE)
  cdm::AddAndroidPlatformKeySystems(key_systems_properties);
}

}  // namespace android_webview
