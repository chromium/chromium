// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_REGISTRATION_H_
#define ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_REGISTRATION_H_

#include "base/functional/callback_forward.h"

namespace component_updater {
struct ComponentRegistration;
}  // namespace component_updater

namespace android_webview {
void RegisterComponentsForUpdate(
    base::RepeatingCallback<bool(
        const component_updater::ComponentRegistration&)> register_callback,
    base::OnceClosure on_finished);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_REGISTRATION_H_
