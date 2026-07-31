// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_extension_bridge.h"

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_extension_binder_provider.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_extension_bridge_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/browser/extensions_browser_client.h"

namespace contextual_tasks {

ContextualTasksExtensionBridge::ContextualTasksExtensionBridge(Profile* profile)
    : profile_(*profile) {
  ContextualTasksExtensionBinderProvider::Register(profile);
  auto* client = extensions::ExtensionsBrowserClient::Get();
  CHECK(client) << "ExtensionsBrowserClient must exist.";
  auto* resource_manager = client->GetComponentExtensionResourceManager();
  CHECK(resource_manager) << "ComponentExtensionResourceManager must exist.";
  load_time_data_subscription_ = resource_manager->RegisterTemplateDataProvider(
      extension_misc::kContextualTasksExtensionId, &profile_.get(),
      base::BindRepeating(&ContextualTasksExtensionBridge::GetLoadTimeData,
                          base::Unretained(this)));
}

ContextualTasksExtensionBridge::~ContextualTasksExtensionBridge() = default;

base::DictValue ContextualTasksExtensionBridge::GetLoadTimeData() {
  return ContextualTasksUI::GetContextualTasksLoadTimeData(&profile_.get());
}

// static
ContextualTasksExtensionBridge* ContextualTasksExtensionBridge::Get(
    Profile* profile) {
  return ContextualTasksExtensionBridgeFactory::GetForProfile(profile);
}

}  // namespace contextual_tasks
