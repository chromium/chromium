// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/chrome_rules_service.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

#include "chrome/browser/glic/host/guest_util.h"

namespace data_controls {

// ---------------------------------
// ChromeRulesService implementation
// ---------------------------------

ChromeRulesService::ChromeRulesService(content::BrowserContext* browser_context)
    : RulesServiceBase(
          Profile::FromBrowserContext(browser_context)->GetPrefs()),
      profile_(Profile::FromBrowserContext(browser_context)) {}

ChromeRulesService::~ChromeRulesService() = default;

Verdict ChromeRulesService::GetPrintVerdict(
    const GURL& printed_page_url) const {
  return GetVerdict(Rule::Restriction::kPrinting, {.source = {
                                                       .url = printed_page_url,
                                                   }});
}

Verdict ChromeRulesService::GetPasteVerdict(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const ui::ClipboardMetadata& metadata) const {
  base::ScopedUmaHistogramTimer timer(
      "Enterprise.DataControls.Paste.EvaluationLatency");
  return GetVerdict(Rule::Restriction::kClipboard,
                    {
                        .source = GetAsActionSource(source, metadata),
                        .destination = GetAsActionDestination(destination),
                    });
}

bool ChromeRulesService::incognito_profile() const {
  return profile_->IsIncognitoProfile();
}

ActionSource ChromeRulesService::GetAsActionSource(
    const content::ClipboardEndpoint& endpoint,
    const ui::ClipboardMetadata& metadata) const {
  ActionSource action;
  if (!endpoint.browser_context()) {
    action.os_clipboard = true;
  } else {
    action = ExtractPasteActionContext<ActionSource>(endpoint);
  }

  if (metadata.size.has_value()) {
    action.content_size = base::saturated_cast<int64_t>(*metadata.size);
  }

  return action;
}

ActionDestination ChromeRulesService::GetAsActionDestination(
    const content::ClipboardEndpoint& endpoint) const {
  return ExtractPasteActionContext<ActionDestination>(endpoint);
}

template <typename ActionSourceOrDestination>
ActionSourceOrDestination ChromeRulesService::ExtractPasteActionContext(
    const content::ClipboardEndpoint& endpoint) const {
  ActionSourceOrDestination action;
  if (endpoint.data_transfer_endpoint() &&
      endpoint.data_transfer_endpoint()->IsUrlType() &&
      endpoint.data_transfer_endpoint()->GetURL()) {
    action.url = *endpoint.data_transfer_endpoint()->GetURL();
  }
  if (endpoint.browser_context()) {
    action.incognito = Profile::FromBrowserContext(endpoint.browser_context())
                           ->IsIncognitoProfile();
    action.other_profile = endpoint.browser_context() != profile_;
  }
  if (endpoint.web_contents() && (glic::IsGlicGuest(endpoint.web_contents()) ||
                                  glic::IsGlicWebUI(endpoint.web_contents()))) {
    action.gemini_in_chrome = true;
  }
  return action;
}

// ----------------------------------------
// ChromeRulesServiceFactory implementation
// ----------------------------------------

ChromeRulesService* ChromeRulesServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ChromeRulesService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
ChromeRulesServiceFactory* ChromeRulesServiceFactory::GetInstance() {
  static base::NoDestructor<ChromeRulesServiceFactory> instance;
  return instance.get();
}

ChromeRulesServiceFactory::ChromeRulesServiceFactory()
    : ProfileKeyedServiceFactory(
          "DataControlsRulesService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .WithSystem(ProfileSelection::kOwnInstance)
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  // TODO: Add DependsOn statements.
}

ChromeRulesServiceFactory::~ChromeRulesServiceFactory() = default;

std::unique_ptr<KeyedService>
ChromeRulesServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return base::WrapUnique(new ChromeRulesService(context));
}

}  // namespace data_controls
