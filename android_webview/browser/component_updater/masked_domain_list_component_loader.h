// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_COMPONENT_UPDATER_MASKED_DOMAIN_LIST_COMPONENT_LOADER_H_
#define ANDROID_WEBVIEW_BROWSER_COMPONENT_UPDATER_MASKED_DOMAIN_LIST_COMPONENT_LOADER_H_

#include <memory>
#include <vector>

namespace component_updater {
class ComponentLoaderPolicy;
}  // namespace component_updater

namespace android_webview {

using ComponentLoaderPolicyVector =
    std::vector<std::unique_ptr<component_updater::ComponentLoaderPolicy>>;

void LoadMaskedDomainListComponent(ComponentLoaderPolicyVector& policies);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_COMPONENT_UPDATER_MASKED_DOMAIN_LIST_COMPONENT_LOADER_H_
