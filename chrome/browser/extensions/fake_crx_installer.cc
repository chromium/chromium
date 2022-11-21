// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/fake_crx_installer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

FakeCrxInstaller::FakeCrxInstaller(ExtensionService* frontend)
    : CrxInstaller(frontend->AsExtensionServiceWeakPtr(), nullptr, nullptr) {}

FakeCrxInstaller::~FakeCrxInstaller() = default;

void FakeCrxInstaller::InstallCrxFile(const CRXFileInfo& info) {}

}  // namespace extensions
