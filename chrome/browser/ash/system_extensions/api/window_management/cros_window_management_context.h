// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_WINDOW_MANAGEMENT_CROS_WINDOW_MANAGEMENT_CONTEXT_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_WINDOW_MANAGEMENT_CROS_WINDOW_MANAGEMENT_CONTEXT_H_

#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace ash {

// Class in charge of managing CrosWindowManagement instances and dispatching
// events to them.
class CrosWindowManagementContext : public KeyedService {
 public:
  // Returns the event dispatcher associated with `profile`. Should only be
  // called if System Extensions is enabled for the profile i.e. if
  // IsSystemExtensionsEnabled() returns true.
  static CrosWindowManagementContext& Get(Profile* profile);

  CrosWindowManagementContext();
  CrosWindowManagementContext(const CrosWindowManagementContext&) = delete;
  CrosWindowManagementContext& operator=(const CrosWindowManagementContext&) =
      delete;
  ~CrosWindowManagementContext() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_WINDOW_MANAGEMENT_CROS_WINDOW_MANAGEMENT_CONTEXT_H_
