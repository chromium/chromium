// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_MASKED_DOMAIN_LIST_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_MASKED_DOMAIN_LIST_COMPONENT_INSTALLER_H_

#include <optional>

#include "mojo/public/cpp/base/proto_wrapper.h"

namespace base {
class Version;
}

namespace component_updater {

class ComponentUpdateService;

void OnMaskedDomainListReady(
    base::Version version,
    std::optional<mojo_base::ProtoWrapper> masked_domain_list);
void RegisterMaskedDomainListComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_MASKED_DOMAIN_LIST_COMPONENT_INSTALLER_H_
