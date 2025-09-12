// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_MASKED_DOMAIN_LIST_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_MASKED_DOMAIN_LIST_COMPONENT_INSTALLER_H_

#include <optional>

#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"

namespace base {
class Version;
}

namespace component_updater {

class ComponentUpdateService;

void OnMaskedDomainListReady(
    base::Version version,
    std::optional<masked_domain_list::MaskedDomainList> masked_domain_list);
void RegisterMaskedDomainListComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_MASKED_DOMAIN_LIST_COMPONENT_INSTALLER_H_
