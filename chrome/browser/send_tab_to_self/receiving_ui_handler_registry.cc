// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/receiving_ui_handler_registry.h"

#include <vector>

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/receiving_ui_handler.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_button_controller.h"
#include "components/send_tab_to_self/features.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) || \
    defined(OS_WIN)
#include "chrome/browser/send_tab_to_self/desktop_notification_handler.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/android/send_tab_to_self/android_notification_handler.h"
#endif

namespace send_tab_to_self {

ReceivingUiHandlerRegistry::ReceivingUiHandlerRegistry() {}
ReceivingUiHandlerRegistry::~ReceivingUiHandlerRegistry() {}

// static
ReceivingUiHandlerRegistry* ReceivingUiHandlerRegistry::GetInstance() {
  return base::Singleton<ReceivingUiHandlerRegistry>::get();
}

// Instantiates all the handlers relevant to this platform.
void ReceivingUiHandlerRegistry::InstantiatePlatformSpecificHandlers(
    Profile* profile) {
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) || \
    defined(OS_WIN)

  // If STTS 2.0 is enabled the handler will be created when the toolbar
  // button registers itself as the delegate.
  if (!base::FeatureList::IsEnabled(kSendTabToSelfV2)) {
    applicable_handlers_.push_back(
        std::make_unique<send_tab_to_self::DesktopNotificationHandler>(
            profile));
  }
#elif defined(OS_ANDROID)
  applicable_handlers_.push_back(
      std::make_unique<AndroidNotificationHandler>());
#endif
}

SendTabToSelfToolbarButtonController*
ReceivingUiHandlerRegistry::GetToolbarButtonControllerForProfile(
    Profile* profile) {
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) || \
    defined(OS_WIN)
  for (const std::unique_ptr<ReceivingUiHandler>& handler :
       applicable_handlers_) {
    auto* button_controller =
        static_cast<SendTabToSelfToolbarButtonController*>(handler.get());
    if (button_controller && button_controller->profile() == profile) {
      return button_controller;
    }
  }

  applicable_handlers_.push_back(
      std::make_unique<SendTabToSelfToolbarButtonController>(profile));
  auto* button_controller = static_cast<SendTabToSelfToolbarButtonController*>(
      applicable_handlers_.back().get());
  return button_controller;
#elif defined(OS_ANDROID)
  return nullptr;
#endif
}

const std::vector<std::unique_ptr<ReceivingUiHandler>>&
ReceivingUiHandlerRegistry::GetHandlers() const {
  return applicable_handlers_;
}

}  // namespace send_tab_to_self
