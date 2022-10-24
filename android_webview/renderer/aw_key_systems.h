// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_KEY_SYSTEMS_H_
#define ANDROID_WEBVIEW_RENDERER_AW_KEY_SYSTEMS_H_

#include "media/base/key_system_info.h"

namespace android_webview {

void AwAddKeySystems(media::KeySystemInfos* key_systems_properties);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_AW_KEY_SYSTEMS_H_
