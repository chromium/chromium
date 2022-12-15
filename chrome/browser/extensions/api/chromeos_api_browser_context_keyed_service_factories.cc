// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/chromeos_api_browser_context_keyed_service_factories.h"

#include "chrome/browser/chromeos/extensions/login_screen/login/external_logout_request/external_logout_request_event_handler_factory.h"
#include "chrome/browser/chromeos/extensions/login_screen/login_state/session_state_changed_event_dispatcher.h"
#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service_factory.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/chromeos/extensions/login_screen/login/external_logout_done/external_logout_done_event_handler_factory.h"
#endif

#if BUILDFLAG(USE_CUPS)
#include "chrome/browser/chromeos/extensions/printing_metrics/printing_metrics_service.h"
#endif

namespace chromeos_extensions {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  extensions::ExternalLogoutDoneEventHandlerFactory::GetInstance();
#endif
  extensions::ExternalLogoutRequestEventHandlerFactory::GetInstance();
#if BUILDFLAG(USE_CUPS)
  extensions::PrintingMetricsService::GetFactoryInstance();
#endif
  extensions::SessionStateChangedEventDispatcher::GetFactoryInstance();
  chromeos::VpnServiceFactory::GetInstance();
}

}  // namespace chromeos_extensions
