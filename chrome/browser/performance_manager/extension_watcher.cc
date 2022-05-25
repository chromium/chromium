// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/extension_watcher.h"

#include "chrome/browser/browser_process.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "extensions/browser/extension_host.h"

namespace performance_manager {

ExtensionWatcher::ExtensionWatcher() {
  profile_manager_observation_.Observe(g_browser_process->profile_manager());
}

ExtensionWatcher::~ExtensionWatcher() = default;

void ExtensionWatcher::OnProfileAdded(Profile* profile) {
  extension_process_manager_observation_.AddObservation(
      extensions::ProcessManager::Get(profile));
}

void ExtensionWatcher::OnBackgroundHostCreated(
    extensions::ExtensionHost* host) {
  auto* registry = PerformanceManagerRegistry::GetInstance();
  DCHECK(registry);
  registry->SetPageType(host->host_contents(), PageType::kExtension);
}

void ExtensionWatcher::OnProcessManagerShutdown(
    extensions::ProcessManager* manager) {
  extension_process_manager_observation_.RemoveObservation(manager);
}

}  // namespace performance_manager
