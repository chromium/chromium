// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_extension_bridge.h"

#include "chrome/browser/contextual_tasks/contextual_tasks_extension_binder_provider.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_extension_bridge_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_extension_config_provider.h"
#include "chrome/browser/profiles/profile.h"

namespace contextual_tasks {

ContextualTasksExtensionBridge::ContextualTasksExtensionBridge(
    Profile* profile) {
  ContextualTasksExtensionConfigProvider::Register(profile);
  ContextualTasksExtensionBinderProvider::Register(profile);
}

ContextualTasksExtensionBridge::~ContextualTasksExtensionBridge() = default;

// static
ContextualTasksExtensionBridge* ContextualTasksExtensionBridge::Get(
    Profile* profile) {
  return ContextualTasksExtensionBridgeFactory::GetForProfile(profile);
}

}  // namespace contextual_tasks
