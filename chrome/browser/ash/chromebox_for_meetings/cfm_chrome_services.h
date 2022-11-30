// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_CFM_CHROME_SERVICES_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_CFM_CHROME_SERVICES_H_

#include "base/observer_list_types.h"

namespace ash::cfm {

// Registers the observers for service interface requests by the
// |CfmServiceContext| granting hotline access to services.
void InitializeCfmServices();

// Removes the observers for service interface requests by the
// |CfmServiceContext| removing hotline access to services.
void ShutdownCfmServices();

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_CFM_CHROME_SERVICES_H_
