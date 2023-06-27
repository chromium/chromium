// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_MASKED_DOMAIN_LIST_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_MASKED_DOMAIN_LIST_COMPONENT_INSTALLER_H_

#include "components/component_updater/component_installer.h"

namespace component_updater {

void RegisterMaskedDomainListComponent(ComponentUpdateService* cus);

}

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_MASKED_DOMAIN_LIST_COMPONENT_INSTALLER_H_
