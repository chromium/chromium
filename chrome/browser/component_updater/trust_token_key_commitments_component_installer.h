// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_TRUST_TOKEN_KEY_COMMITMENTS_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_TRUST_TOKEN_KEY_COMMITMENTS_COMPONENT_INSTALLER_H_

#include "components/component_updater/component_installer.h"

namespace component_updater {

class ComponentUpdateService;

// Call once during startup to make the component update service aware of
// the component.
void RegisterTrustTokenKeyCommitmentsComponentIfTrustTokensEnabled(
    ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_TRUST_TOKEN_KEY_COMMITMENTS_COMPONENT_INSTALLER_H_
