// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_VERSION_INFO_ANDROID_CHANNEL_GETTER_H_
#define BASE_VERSION_INFO_ANDROID_CHANNEL_GETTER_H_

#include "base/base_export.h"
#include "base/version_info/channel.h"

namespace version_info {
namespace android {

BASE_EXPORT Channel GetChannel();
BASE_EXPORT void SetChannel(Channel channel);

}  // namespace android
}  // namespace version_info

#endif  // BASE_VERSION_INFO_ANDROID_CHANNEL_GETTER_H_
