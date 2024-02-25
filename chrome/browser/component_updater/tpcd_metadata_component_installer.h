// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_TPCD_METADATA_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_TPCD_METADATA_COMPONENT_INSTALLER_H_

#include "components/component_updater/component_installer.h"

namespace component_updater {

// Called once during startup to make the component updater service aware of
// the TPCD Metadata component.
void RegisterTpcdMetadataComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_TPCD_METADATA_COMPONENT_INSTALLER_H_
