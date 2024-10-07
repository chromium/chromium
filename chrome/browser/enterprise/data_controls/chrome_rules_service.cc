// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/chrome_rules_service.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/data_controls/core/browser/features.h"
#include "components/enterprise/data_controls/core/browser/prefs.h"
#include "components/prefs/pref_service.h"

namespace data_controls {

// ---------------------------------
// ChromeRulesService implementation
// ---------------------------------

ChromeRulesService::ChromeRulesService(content::BrowserContext* browser_context)
    : RulesService(Profile::FromBrowserContext(browser_context)->GetPrefs()),
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
    const content::ClipboardMetadata& metadata) const {
  return GetVerdict(Rule::Restriction::kClipboard,
                    {
                        .source = GetAsActionSource(source),
                        .destination = GetAsActionDestination(destination),
                    });
}

Verdict ChromeRulesService::GetCopyRestrictedBySourceVerdict(
    const GURL& source) const {
  return GetVerdict(
      Rule::Restriction::kClipboard,
      {.source = {.url = source, .incognito = profile_->IsIncognitoProfile()}});
}

Verdict ChromeRulesService::GetCopyToOSClipboardVerdict(
    const GURL& source) const {
  return GetVerdict(Rule::Restriction::kClipboard,
                    {
                        .source = {.url = source,
                                   .incognito = profile_->IsIncognitoProfile()},
                        .destination =
                            {
                                .os_clipboard = true,
                            },
                    });
}

bool ChromeRulesService::BlockScreenshots(const GURL& url) const {
  return GetVerdict(Rule::Restriction::kScreenshot,
                    {.source = {.url = url,
                                .incognito = profile_->IsIncognitoProfile()}})
             .level() == Rule::Level::kBlock;
}

ActionSource ChromeRulesService::GetAsActionSource(
    const content::ClipboardEndpoint& endpoint) const {
  if (!endpoint.browser_context()) {
    return {.os_clipboard = true};
  }

  return ExtractPasteActionContext<ActionSource>(endpoint);
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
  return action;
}

// ----------------------------------------
// ChromeRulesServiceFactory implementation
// ----------------------------------------

RulesService* ChromeRulesServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<RulesService*>(
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
