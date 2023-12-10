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
