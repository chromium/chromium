// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_IWA_KEY_DISTRIBUTION_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_IWA_KEY_DISTRIBUTION_COMPONENT_INSTALLER_H_

#include "base/feature_list.h"



namespace component_updater {

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_DECLARE_FEATURE(kIwaKeyDistributionComponent);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

class ComponentUpdateService;


// Called once during startup to make the component update service aware of
// the IWA Key Distribution component.
void RegisterIwaKeyDistributionComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_IWA_KEY_DISTRIBUTION_COMPONENT_INSTALLER_H_
