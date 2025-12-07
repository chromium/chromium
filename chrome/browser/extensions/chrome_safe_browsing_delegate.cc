// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_safe_browsing_delegate.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_password_reuse_detection_manager_client.h"
#include "chrome/browser/safe_browsing/extension_telemetry/declarative_net_request_action_signal.h"
#include "chrome/browser/safe_browsing/extension_telemetry/declarative_net_request_signal.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service_factory.h"
#include "chrome/browser/safe_browsing/extension_telemetry/remote_host_contacted_signal.h"
#include "chrome/browser/safe_browsing/extension_telemetry/tabs_execute_script_signal.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/features.h"

static_assert(BUILDFLAG(FULL_SAFE_BROWSING));

namespace extensions {

ChromeSafeBrowsingDelegate::ChromeSafeBrowsingDelegate() = default;

ChromeSafeBrowsingDelegate::~ChromeSafeBrowsingDelegate() = default;

bool ChromeSafeBrowsingDelegate::IsExtensionTelemetryServiceEnabled(
    content::BrowserContext* context) const {
  auto* telemetry_service =
      safe_browsing::ExtensionTelemetryServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  return telemetry_service && telemetry_service->enabled();
}

void ChromeSafeBrowsingDelegate::NotifyExtensionApiTabExecuteScript(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    const std::string& code) const {
  auto* telemetry_service =
      safe_browsing::ExtensionTelemetryServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  if (!telemetry_service || !telemetry_service->enabled()) {
    return;
  }

  auto signal = std::make_unique<safe_browsing::TabsExecuteScriptSignal>(
      extension_id, code);
  telemetry_service->AddSignal(std::move(signal));
}

void ChromeSafeBrowsingDelegate::NotifyExtensionApiDeclarativeNetRequest(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    const std::vector<api::declarative_net_request::Rule>& rules) const {
  auto* telemetry_service =
      safe_browsing::ExtensionTelemetryServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  if (!telemetry_service || !telemetry_service->enabled()) {
    return;
  }

  // The telemetry service will consume and release the signal object inside the
  // `AddSignal()` call.
  auto signal = std::make_unique<safe_browsing::DeclarativeNetRequestSignal>(
      extension_id, rules);
  telemetry_service->AddSignal(std::move(signal));
}

void ChromeSafeBrowsingDelegate::
    NotifyExtensionDeclarativeNetRequestRedirectAction(
        content::BrowserContext* context,
        const ExtensionId& extension_id,
        const GURL& request_url,
        const GURL& redirect_url) const {
  auto* telemetry_service =
      safe_browsing::ExtensionTelemetryServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  if (!telemetry_service || !telemetry_service->enabled() ||
      !base::FeatureList::IsEnabled(
          safe_browsing::
              kExtensionTelemetryDeclarativeNetRequestActionSignal)) {
    return;
  }

  // The telemetry service will consume and release the signal object inside the
  // `AddSignal()` call.
  auto signal = safe_browsing::DeclarativeNetRequestActionSignal::
      CreateDeclarativeNetRequestRedirectActionSignal(extension_id, request_url,
                                                      redirect_url);
  telemetry_service->AddSignal(std::move(signal));
}

void ChromeSafeBrowsingDelegate::CreatePasswordReuseDetectionManager(
    content::WebContents* web_contents) const {
  ChromePasswordReuseDetectionManagerClient::CreateForWebContents(web_contents);
}

}  // namespace extensions
