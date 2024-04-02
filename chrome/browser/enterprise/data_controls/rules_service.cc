// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/rules_service.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/data_controls/features.h"
#include "components/enterprise/data_controls/prefs.h"
#include "components/prefs/pref_service.h"

namespace data_controls {

// ---------------------------
// RulesService implementation
// ---------------------------

RulesService::RulesService(content::BrowserContext* browser_context)
    : profile_(Profile::FromBrowserContext(browser_context)) {
  if (base::FeatureList::IsEnabled(kEnableDesktopDataControls)) {
    pref_registrar_.Init(profile_->GetPrefs());
    pref_registrar_.Add(
        kDataControlsRulesPref,
        base::BindRepeating(&RulesService::OnDataControlsRulesUpdate,
                            base::Unretained(this)));
    OnDataControlsRulesUpdate();
  }
}

RulesService::~RulesService() = default;

Verdict RulesService::GetPrintVerdict(const GURL& printed_page_url) const {
  return GetVerdict(Rule::Restriction::kPrinting, {.source = {
                                                       .url = printed_page_url,
                                                   }});
}

Verdict RulesService::GetPasteVerdict(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const content::ClipboardMetadata& metadata) const {
  return GetVerdict(Rule::Restriction::kClipboard,
                    {
                        .source = GetAsActionSource(source),
                        .destination = GetAsActionDestination(destination),
                    });
}

Verdict RulesService::GetCopyRestrictedBySourceVerdict(
    const GURL& source) const {
  return GetVerdict(
      Rule::Restriction::kClipboard,
      {.source = {.url = source, .incognito = profile_->IsIncognitoProfile()}});
}

Verdict RulesService::GetCopyToOSClipboardVerdict(const GURL& source) const {
  return GetVerdict(
      Rule::Restriction::kClipboard,
      {.source = {.url = source, .incognito = profile_->IsIncognitoProfile()},
       .destination = {.os_clipboard = true}});
}

bool RulesService::BlockScreenshots(const GURL& url) const {
  return GetVerdict(Rule::Restriction::kScreenshot,
                    {.source = {.url = url,
                                .incognito = profile_->IsIncognitoProfile()}})
             .level() == Rule::Level::kBlock;
}

Verdict RulesService::GetVerdict(Rule::Restriction restriction,
                                 const ActionContext& context) const {
  if (!base::FeatureList::IsEnabled(kEnableDesktopDataControls)) {
    return Verdict::NotSet();
  }

  Rule::Level max_level = Rule::Level::kNotSet;
  Verdict::TriggeredRules triggered_rules;
  for (const auto& rule : rules_) {
    Rule::Level level = rule.GetLevel(restriction, context);
    if (level > max_level) {
      max_level = level;
    }
    if (level != Rule::Level::kNotSet && !rule.rule_id().empty()) {
      triggered_rules[rule.rule_id()] = rule.name();
    }
  }

  switch (max_level) {
    case Rule::Level::kNotSet:
      return Verdict::NotSet();
    case Rule::Level::kReport:
      return Verdict::Report(std::move(triggered_rules));
    case Rule::Level::kWarn:
      return Verdict::Warn(std::move(triggered_rules));
    case Rule::Level::kBlock:
      return Verdict::Block(std::move(triggered_rules));
    case Rule::Level::kAllow:
      return Verdict::Allow();
  }
}

ActionSource RulesService::GetAsActionSource(
    const content::ClipboardEndpoint& endpoint) const {
  if (!endpoint.browser_context()) {
    return {.os_clipboard = true};
  }

  return ExtractPasteActionContext<ActionSource>(endpoint);
}

ActionDestination RulesService::GetAsActionDestination(
    const content::ClipboardEndpoint& endpoint) const {
  return ExtractPasteActionContext<ActionDestination>(endpoint);
}

template <typename ActionSourceOrDestination>
ActionSourceOrDestination RulesService::ExtractPasteActionContext(
    const content::ClipboardEndpoint& endpoint) const {
  ActionSourceOrDestination action;
  if (endpoint.data_transfer_endpoint() &&
      endpoint.data_transfer_endpoint()->IsUrlType()) {
    action.url = *endpoint.data_transfer_endpoint()->GetURL();
  }
  if (endpoint.browser_context()) {
    action.incognito = Profile::FromBrowserContext(endpoint.browser_context())
                           ->IsIncognitoProfile();
    action.other_profile = endpoint.browser_context() != profile_;
  }
  return action;
}

void RulesService::OnDataControlsRulesUpdate() {
  DCHECK(profile_);
  if (!base::FeatureList::IsEnabled(kEnableDesktopDataControls)) {
    return;
  }

  rules_.clear();

  const base::Value::List& rules_list =
      profile_->GetPrefs()->GetList(kDataControlsRulesPref);

  for (const base::Value& rule_value : rules_list) {
    auto rule = Rule::Create(rule_value);

    if (!rule) {
      continue;
    }

    rules_.push_back(std::move(*rule));
  }
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
