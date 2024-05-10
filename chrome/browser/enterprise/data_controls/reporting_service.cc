// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/reporting_service.h"

#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/data_controls/prefs.h"
#include "components/enterprise/data_controls/verdict.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/clipboard_types.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_format_type.h"

namespace data_controls {

namespace {

// TODO(b/311679168): Move this to share logic with ContentAnalysisDelegate.
std::string GetMimeType(const ui::ClipboardFormatType& clipboard_format) {
  if (clipboard_format == ui::ClipboardFormatType::PlainTextType()) {
    return ui::kMimeTypeText;
  } else if (clipboard_format == ui::ClipboardFormatType::HtmlType()) {
    return ui::kMimeTypeHTML;
  } else if (clipboard_format == ui::ClipboardFormatType::SvgType()) {
    return ui::kMimeTypeSvg;
  } else if (clipboard_format == ui::ClipboardFormatType::RtfType()) {
    return ui::kMimeTypeRTF;
  } else if (clipboard_format == ui::ClipboardFormatType::PngType()) {
    return ui::kMimeTypePNG;
  } else if (clipboard_format == ui::ClipboardFormatType::FilenamesType()) {
    return ui::kMimeTypeURIList;
  }
  return "";
}

GURL GetURL(const content::ClipboardEndpoint& endpoint) {
  if (!endpoint.data_transfer_endpoint() ||
      !endpoint.data_transfer_endpoint()->IsUrlType() ||
      !endpoint.data_transfer_endpoint()->GetURL()) {
    return GURL();
  }
  return *endpoint.data_transfer_endpoint()->GetURL();
}

safe_browsing::EventResult GetEventResult(Rule::Level level) {
  switch (level) {
    case Rule::Level::kNotSet:
    case Rule::Level::kAllow:
    case Rule::Level::kReport:
      return safe_browsing::EventResult::ALLOWED;
    case Rule::Level::kBlock:
      return safe_browsing::EventResult::BLOCKED;
    case Rule::Level::kWarn:
      return safe_browsing::EventResult::WARNED;
  }
}

}  // namespace

// -------------------------------
// ReportingService implementation
// -------------------------------

ReportingService::ReportingService(content::BrowserContext& browser_context)
    : profile_(*Profile::FromBrowserContext(&browser_context)) {}

ReportingService::~ReportingService() = default;

void ReportingService::ReportPaste(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const content::ClipboardMetadata& metadata,
    const Verdict& verdict) {
  ReportPaste(
      source, destination, metadata, verdict,
      extensions::SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
      GetEventResult(verdict.level()));
}

void ReportingService::ReportPasteWarningBypass(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const content::ClipboardMetadata& metadata,
    const Verdict& verdict) {
  ReportPaste(
      source, destination, metadata, verdict,
      extensions::SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
      safe_browsing::EventResult::BYPASSED);
}

void ReportingService::ReportPaste(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const content::ClipboardMetadata& metadata,
    const Verdict& verdict,
    const std::string& trigger,
    safe_browsing::EventResult event_result) {
  auto* router =
      extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(
          &profile_.get());

  if (!router || verdict.triggered_rules().empty()) {
    return;
  }

  GURL source_url =
      IncludeSourceInformation(source, destination) ? GetURL(source) : GURL();
  GURL paste_url = GetURL(destination);

  router->OnDataControlsSensitiveDataEvent(
      /*url=*/paste_url,
      /*tab_url=*/paste_url,
      /*source=*/source_url.spec(),
      /*destination=*/paste_url.spec(),
      /*mime_type=*/GetMimeType(metadata.format_type),
      /*trigger=*/trigger,
      /*triggered_rules=*/verdict.triggered_rules(),
      /*event_result=*/event_result,
      /*content_size=*/metadata.size.value_or(-1));
}

bool ReportingService::IncludeSourceInformation(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination) const {
  if (source.browser_context() && source.browser_context()->IsOffTheRecord()) {
    return false;
  }

  // When the source is not an incognito tab, there are two cases where
  // `source`'s information can be included in reported events:
  // - When copy-pasting happens within the same profile.
  // - When the DC rules are being applied to the whole browser.
  return source.browser_context() == destination.browser_context() ||
         profile_->GetPrefs()->GetInteger(kDataControlsRulesScopePref) ==
             policy::PolicyScope::POLICY_SCOPE_MACHINE;
}

// --------------------------------------
// ReportingServiceFactory implementation
// --------------------------------------

// static
ReportingService* ReportingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ReportingService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
ReportingServiceFactory* ReportingServiceFactory::GetInstance() {
  static base::NoDestructor<ReportingServiceFactory> instance;
  return instance.get();
}

ReportingServiceFactory::ReportingServiceFactory()
    : ProfileKeyedServiceFactory(
          "DataControlsReportingService",
          // `kOriginalOnly` is used since there is no reporting done for
          // incognito profiles.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(extensions::SafeBrowsingPrivateEventRouterFactory::GetInstance());
}

ReportingServiceFactory::~ReportingServiceFactory() = default;

std::unique_ptr<KeyedService>
ReportingServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return base::WrapUnique(new ReportingService(*context));
}

}  // namespace data_controls
