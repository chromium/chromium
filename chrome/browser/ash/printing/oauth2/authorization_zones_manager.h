// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_ZONES_MANAGER_H_
#define CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_ZONES_MANAGER_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace ash {
namespace printing {
namespace oauth2 {

class AuthorizationZonesManager : public KeyedService {
 public:
  static std::unique_ptr<AuthorizationZonesManager> Create(Profile* profile);
  ~AuthorizationZonesManager() override;

 protected:
  AuthorizationZonesManager();
};

}  // namespace oauth2
}  // namespace printing
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_ZONES_MANAGER_H_
