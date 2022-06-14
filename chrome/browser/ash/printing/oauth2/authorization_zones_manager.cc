// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"

namespace ash {
namespace printing {
namespace oauth2 {

namespace {

class AuthorizationZonesManagerImpl : public AuthorizationZonesManager {
 public:
  explicit AuthorizationZonesManagerImpl(Profile* profile) {}
};

}  // namespace

std::unique_ptr<AuthorizationZonesManager> AuthorizationZonesManager::Create(
    Profile* profile) {
  return std::make_unique<AuthorizationZonesManagerImpl>(profile);
}

AuthorizationZonesManager::~AuthorizationZonesManager() = default;

AuthorizationZonesManager::AuthorizationZonesManager() = default;

}  // namespace oauth2
}  // namespace printing
}  // namespace ash
