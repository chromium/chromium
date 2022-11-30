// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/registry_interface.h"

namespace ash {
namespace file_system_provider {

RegistryInterface::~RegistryInterface() {
}

RegistryInterface::RestoredFileSystem::RestoredFileSystem() {
}

RegistryInterface::RestoredFileSystem::RestoredFileSystem(
    const RestoredFileSystem& other) = default;

RegistryInterface::RestoredFileSystem::~RestoredFileSystem() {
}

}  // namespace file_system_provider
}  // namespace ash
