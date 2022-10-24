// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_key_systems.h"
#include "components/cdm/renderer/android_key_systems.h"

namespace android_webview {

void AwAddKeySystems(media::KeySystemInfos* key_systems_infos) {
#if BUILDFLAG(ENABLE_WIDEVINE)
  cdm::AddAndroidWidevine(key_systems_infos);
#endif  // BUILDFLAG(ENABLE_WIDEVINE)
  cdm::AddAndroidPlatformKeySystems(key_systems_infos);
}

}  // namespace android_webview
