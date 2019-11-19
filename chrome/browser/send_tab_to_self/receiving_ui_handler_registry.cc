// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/receiving_ui_handler_registry.h"

#include <vector>

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/receiving_ui_handler.h"

#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
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
#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
  applicable_handlers_.push_back(
      std::make_unique<send_tab_to_self::DesktopNotificationHandler>(profile));
#elif defined(OS_ANDROID)
  applicable_handlers_.push_back(
      std::make_unique<AndroidNotificationHandler>());
#endif
}

const std::vector<std::unique_ptr<ReceivingUiHandler>>&
ReceivingUiHandlerRegistry::GetHandlers() const {
  return applicable_handlers_;
}

}  // namespace send_tab_to_self
