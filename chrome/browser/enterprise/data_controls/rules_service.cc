// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/rules_service.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"

namespace data_controls {

// ---------------------------
// RulesService implementation
// ---------------------------

RulesService::RulesService(content::BrowserContext* browser_context)
    : rules_manager_(Profile::FromBrowserContext(browser_context)) {}

RulesService::~RulesService() = default;

Verdict RulesService::GetPrintVerdict(const GURL& printed_page_url) const {
  return rules_manager_.GetVerdict(Rule::Restriction::kPrinting,
                                   {.source = {
                                        .url = printed_page_url,
                                    }});
}

Verdict RulesService::GetPasteVerdict(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const content::ClipboardMetadata& metadata) const {
  ActionContext context;
  if (source.data_transfer_endpoint() &&
      source.data_transfer_endpoint()->IsUrlType()) {
    context.source.url = *source.data_transfer_endpoint()->GetURL();
  }
  if (source.browser_context()) {
    context.source.incognito =
        Profile::FromBrowserContext(source.browser_context())
            ->IsIncognitoProfile();
  }
  if (destination.data_transfer_endpoint() &&
      destination.data_transfer_endpoint()->IsUrlType()) {
    context.destination.url = *destination.data_transfer_endpoint()->GetURL();
  }
  if (destination.browser_context()) {
    context.destination.incognito =
        Profile::FromBrowserContext(destination.browser_context())
            ->IsIncognitoProfile();
  }
  return rules_manager_.GetVerdict(Rule::Restriction::kClipboard, context);
}

// ----------------------------------
// RulesServiceFactory implementation
// ----------------------------------

// static
RulesService* RulesServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<RulesService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
RulesServiceFactory* RulesServiceFactory::GetInstance() {
  static base::NoDestructor<RulesServiceFactory> instance;
  return instance.get();
}

RulesServiceFactory::RulesServiceFactory()
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

RulesServiceFactory::~RulesServiceFactory() = default;

std::unique_ptr<KeyedService>
RulesServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return base::WrapUnique(new RulesService(context));
}

}  // namespace data_controls
