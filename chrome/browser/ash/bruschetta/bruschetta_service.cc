// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_service.h"

#include <memory>
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/bruschetta/bruschetta_launcher.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service_factory.h"

namespace bruschetta {

BruschettaService::BruschettaService(Profile* profile) {
  // TODO(b/233289313): Once we have an installer we need to do this
  // dynamically, but in the alpha people have VMs they created via vmc called
  // "bru" so hardcode this to get them working while we work on the full
  // installer.
  launchers_.insert(
      {"bru", std::make_unique<BruschettaLauncher>("bru", profile)});
}

BruschettaService::~BruschettaService() = default;

BruschettaService* BruschettaService::GetForProfile(Profile* profile) {
  return BruschettaServiceFactory::GetForProfile(profile);
}

base::WeakPtr<BruschettaLauncher> BruschettaService::GetLauncher(
    std::string vm_name) {
  auto it = launchers_.find(vm_name);
  if (it == launchers_.end()) {
    return nullptr;
  }
  return it->second->GetWeakPtr();
}

}  // namespace bruschetta
