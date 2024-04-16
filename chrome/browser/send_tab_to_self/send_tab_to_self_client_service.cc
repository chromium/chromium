// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service.h"

#include <memory>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "chrome/browser/send_tab_to_self/desktop_notification_handler.h"
#include "chrome/browser/send_tab_to_self/receiving_ui_handler.h"
#include "chrome/browser/send_tab_to_self/receiving_ui_handler_registry.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_icon_controller.h"
#endif

namespace send_tab_to_self {

SendTabToSelfClientService::SendTabToSelfClientService(
    Profile* profile,
    SendTabToSelfModel* model) {
  model_ = model;
  model_->AddObserver(this);

  profile_ = profile;

  SetupHandlerRegistry(profile);
}

SendTabToSelfClientService::~SendTabToSelfClientService() = default;

void SendTabToSelfClientService::Shutdown() {
  model_->RemoveObserver(this);
  model_ = nullptr;
  registry_->OnProfileShutdown(profile_);
}

void SendTabToSelfClientService::SendTabToSelfModelLoaded() {
  // TODO(crbug.com/40621767): Push changes that happened before the model was
  // loaded.
}

void SendTabToSelfClientService::EntriesAddedRemotely(
    const std::vector<const SendTabToSelfEntry*>& new_entries) {
  for (const std::unique_ptr<ReceivingUiHandler>& handler : GetHandlers()) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
    // Only respond to notifications corresponding to this service's profile
    // for these OSes; mobile does not have a Profile.
    // Cast note: on desktop, handlers are guaranteed to be the derived class
    // DesktopNotificationHandlers outside of test code; see
    // ReceivingUiHandlerRegistry::InstantiatePlatformSpecificHandlers().
    // If modifying this code, modify the method in the unittest as well.
    // TODO(skare): ReceivingUiHandler should be able to filter at its level,
    // or the registry should not be a singleton so we don't need to filter at
    // all. This narrow patch is less risky, but we should make a larger change.
    auto* button_controller =
        static_cast<SendTabToSelfToolbarIconController*>(handler.get());
    if (button_controller && button_controller->profile() == profile_) {
      handler->DisplayNewEntries(new_entries);
    }
#else
    handler->DisplayNewEntries(new_entries);
#endif
  }
}

void SendTabToSelfClientService::EntriesRemovedRemotely(
    const std::vector<std::string>& guids) {
  for (const std::unique_ptr<ReceivingUiHandler>& handler : GetHandlers()) {
    handler->DismissEntries(guids);
  }
}

void SendTabToSelfClientService::SetupHandlerRegistry(Profile* profile) {
  registry_ = ReceivingUiHandlerRegistry::GetInstance();
  registry_->InstantiatePlatformSpecificHandlers(profile);
}

const std::vector<std::unique_ptr<ReceivingUiHandler>>&
SendTabToSelfClientService::GetHandlers() const {
  return registry_->GetHandlers();
}

}  // namespace send_tab_to_self
