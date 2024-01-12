// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/registry_interface.h"

namespace ash::file_system_provider {

RegistryInterface::~RegistryInterface() = default;

RegistryInterface::RestoredFileSystem::RestoredFileSystem() = default;

RegistryInterface::RestoredFileSystem::RestoredFileSystem(
    const RestoredFileSystem& other) = default;

RegistryInterface::RestoredFileSystem::~RestoredFileSystem() = default;

}  // namespace ash::file_system_provider
