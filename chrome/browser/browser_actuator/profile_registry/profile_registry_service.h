// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_ACTUATOR_PROFILE_REGISTRY_PROFILE_REGISTRY_SERVICE_H_
#define CHROME_BROWSER_BROWSER_ACTUATOR_PROFILE_REGISTRY_PROFILE_REGISTRY_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace browser_actuator {

// Per-profile service for the Keychain profile registry.
// TODO(crbug.com/534995082): Publish the profile's registry entry.
class ProfileRegistryService : public KeyedService {
 public:
  ProfileRegistryService();

  ProfileRegistryService(const ProfileRegistryService&) = delete;
  ProfileRegistryService& operator=(const ProfileRegistryService&) = delete;

  ~ProfileRegistryService() override;
};

}  // namespace browser_actuator

#endif  // CHROME_BROWSER_BROWSER_ACTUATOR_PROFILE_REGISTRY_PROFILE_REGISTRY_SERVICE_H_
