// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_REGISTRATION_H_
#define ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_REGISTRATION_H_

namespace component_updater {
class ComponentUpdateService;
}  // namespace component_updater

namespace android_webview {

void RegisterComponentsForUpdate(
    component_updater::ComponentUpdateService* component_update_service);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_REGISTRATION_H_
