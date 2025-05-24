// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_service.h"

#include "chrome/browser/extensions/crx_installer.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;

TestExtensionService::TestExtensionService() = default;

TestExtensionService::~TestExtensionService() = default;

const Extension* TestExtensionService::GetPendingExtensionUpdate(
    const std::string& id) const {
  ADD_FAILURE();
  return nullptr;
}

bool TestExtensionService::FinishDelayedInstallationIfReady(
    const std::string& extension_id,
    bool install_immediately) {
  ADD_FAILURE();
  return false;
}

void TestExtensionService::CheckManagementPolicy() {
  ADD_FAILURE();
}

void TestExtensionService::CheckForUpdatesSoon() {
  ADD_FAILURE();
}

bool TestExtensionService::UserCanDisableInstalledExtension(
    const std::string& extension_id) {
  ADD_FAILURE();
  return false;
}

base::WeakPtr<extensions::ExtensionServiceInterface>
TestExtensionService::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
