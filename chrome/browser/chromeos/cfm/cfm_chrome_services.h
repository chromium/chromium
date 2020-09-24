// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CFM_CFM_CHROME_SERVICES_H_
#define CHROME_BROWSER_CHROMEOS_CFM_CFM_CHROME_SERVICES_H_

#include <string>

#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace chromeos {
namespace cfm {

// Registers the observers for service interface requests by the
// |CfmServiceContext| granting hotline access to services.
void InitializeCfmServices();

}  // namespace cfm
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CFM_CFM_CHROME_SERVICES_H_
