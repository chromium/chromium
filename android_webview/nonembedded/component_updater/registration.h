// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_REGISTRATION_H_
#define ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_REGISTRATION_H_

#include "base/callback_forward.h"
#include "components/update_client/update_client.h"

namespace android_webview {

void RegisterComponentsForUpdate(
    base::RepeatingCallback<bool(const update_client::CrxComponent&)>
        register_callback,
    base::OnceClosure on_finished);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_REGISTRATION_H_
