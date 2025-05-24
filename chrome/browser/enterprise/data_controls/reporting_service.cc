// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/reporting_service.h"

#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/data_controls/core/browser/prefs.h"
#include "components/enterprise/data_controls/core/browser/verdict.h"
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
    return ui::kMimeTypePlainText;
  } else if (clipboard_format == ui::ClipboardFormatType::HtmlType()) {
    return ui::kMimeTypeHtml;
  } else if (clipboard_format == ui::ClipboardFormatType::SvgType()) {
    return ui::kMimeTypeSvg;
  } else if (clipboard_format == ui::ClipboardFormatType::RtfType()) {
    return ui::kMimeTypeRtf;
  } else if (clipboard_format == ui::ClipboardFormatType::PngType()) {
    return ui::kMimeTypePng;
  } else if (clipboard_format == ui::ClipboardFormatType::FilenamesType()) {
    return ui::kMimeTypeUriList;
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

enterprise_connectors::EventResult GetEventResult(Rule::Level level) {
  switch (level) {
    case Rule::Level::kNotSet:
    case Rule::Level::kAllow:
    case Rule::Level::kReport:
      return enterprise_connectors::EventResult::ALLOWED;
    case Rule::Level::kBlock:
      return enterprise_connectors::EventResult::BLOCKED;
    case Rule::Level::kWarn:
      return enterprise_connectors::EventResult::WARNED;
  }
}

bool PolicyAppliedAtUserScope(content::BrowserContext* browser_context,
                              const char* scope_pref) {
  CHECK(browser_context);
  CHECK(scope_pref);

  return Profile::FromBrowserContext(browser_context)
             ->GetPrefs()
             ->GetInteger(scope_pref) == policy::POLICY_SCOPE_USER;
}

}  // namespace

// -------------------------------
// ReportingService implementation
// -------------------------------

// static
enterprise_connectors::ContentMetaData::CopiedTextSource
ReportingService::GetClipboardSource(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const char* scope_pref) {
  CHECK(destination.browser_context());

  using SourceType = enterprise_connectors::ContentMetaData::CopiedTextSource;

  enterprise_connectors::ContentMetaData::CopiedTextSource copied_text_source;
  if (!source.browser_context()) {
    copied_text_source.set_context(SourceType::CLIPBOARD);
  } else if (Profile::FromBrowserContext(source.browser_context())
                 ->IsIncognitoProfile()) {
    copied_text_source.set_context(SourceType::INCOGNITO);
  } else if (source.browser_context() == destination.browser_context()) {
    copied_text_source.set_context(SourceType::SAME_PROFILE);
  } else {
    copied_text_source.set_context(SourceType::OTHER_PROFILE);
  }

  switch (copied_text_source.context()) {
    case SourceType::UNSPECIFIED:
    case SourceType::INCOGNITO:
    case SourceType::CLIPBOARD:
      break;
    case SourceType::OTHER_PROFILE:
      // Only add a source URL if the other profile is getting the policy
      // applied at the machine scope, not the user scope.
      if (PolicyAppliedAtUserScope(destination.browser_context(), scope_pref)) {
        break;
      }
      [[fallthrough]];
    case SourceType::SAME_PROFILE:
      if (source.data_transfer_endpoint() &&
          source.data_transfer_endpoint()->IsUrlType() &&
          source.data_transfer_endpoint()->GetURL()) {
        copied_text_source.set_url(
            source.data_transfer_endpoint()->GetURL()->spec());
      }
      break;
  }

  return copied_text_source;
}

// static
std::string ReportingService::GetClipboardSourceString(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const char* scope_pref) {
  return GetClipboardSourceString(
      GetClipboardSource(source, destination, scope_pref));
}

// static
std::string ReportingService::GetClipboardSourceString(
    const enterprise_connectors::ContentMetaData::CopiedTextSource& source) {
  if (!source.url().empty()) {
    return source.url();
  }

  switch (source.context()) {
    case enterprise_connectors::ContentMetaData::CopiedTextSource::UNSPECIFIED:
    case enterprise_connectors::ContentMetaData::CopiedTextSource::SAME_PROFILE:
      return "";
    case enterprise_connectors::ContentMetaData::CopiedTextSource::INCOGNITO:
      return "INCOGNITO";
    case enterprise_connectors::ContentMetaData::CopiedTextSource::CLIPBOARD:
      return "CLIPBOARD";
    case enterprise_connectors::ContentMetaData::CopiedTextSource::
        OTHER_PROFILE:
      return "OTHER_PROFILE";
  }
}

ReportingService::ReportingService(content::BrowserContext& browser_context)
    : profile_(*Profile::FromBrowserContext(&browser_context)) {}

ReportingService::~ReportingService() = default;

void ReportingService::ReportPaste(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const content::ClipboardMetadata& metadata,
    const Verdict& verdict) {
  ReportCopyOrPaste(
      source, destination, metadata, verdict,
      extensions::SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
      GetEventResult(verdict.level()));
}

void ReportingService::ReportPasteWarningBypassed(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const content::ClipboardMetadata& metadata,
    const Verdict& verdict) {
  ReportCopyOrPaste(
      source, destination, metadata, verdict,
      extensions::SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
      enterprise_connectors::EventResult::BYPASSED);
}

void ReportingService::ReportCopy(const content::ClipboardEndpoint& source,
                                  const content::ClipboardMetadata& metadata,
                                  const Verdict& verdict) {
  ReportCopyOrPaste(
      source, /*destination=*/std::nullopt, metadata, verdict,
      extensions::SafeBrowsingPrivateEventRouter::kTriggerClipboardCopy,
      GetEventResult(verdict.level()));
}

void ReportingService::ReportCopyWarningBypassed(
    const content::ClipboardEndpoint& source,
    const content::ClipboardMetadata& metadata,
    const Verdict& verdict) {
  ReportCopyOrPaste(
      source, /*destination=*/std::nullopt, metadata, verdict,
      extensions::SafeBrowsingPrivateEventRouter::kTriggerClipboardCopy,
      enterprise_connectors::EventResult::BYPASSED);
}

void ReportingService::ReportCopyOrPaste(
    const content::ClipboardEndpoint& source,
    const std::optional<content::ClipboardEndpoint>& destination,
    const content::ClipboardMetadata& metadata,
    const Verdict& verdict,
    const std::string& trigger,
    enterprise_connectors::EventResult event_result) {
  auto* router =
      extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(
          &profile_.get());

  if (!router || verdict.triggered_rules().empty()) {
    return;
  }

  GURL url;
  std::string destination_string;
  std::string source_string;
  if (trigger ==
      extensions::SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload) {
    DCHECK(destination.has_value());

    url = GetURL(*destination);
    destination_string = url.spec();
    source_string = GetClipboardSourceString(source, *destination,
                                             kDataControlsRulesScopePref);
  } else {
    DCHECK_EQ(
        trigger,
        extensions::SafeBrowsingPrivateEventRouter::kTriggerClipboardCopy);
    DCHECK(!destination.has_value());

    url = GetURL(source);
    source_string = GetURL(source).spec();
  }

  router->OnDataControlsSensitiveDataEvent(
      /*url=*/url,
      /*tab_url=*/url,
      /*source=*/source_string,
      /*destination=*/destination_string,
      /*mime_type=*/GetMimeType(metadata.format_type),
      /*trigger=*/trigger,
      /*triggered_rules=*/verdict.triggered_rules(),
      /*event_result=*/event_result,
      /*content_size=*/metadata.size.value_or(-1));
}

// --------------------------------------
// ReportingServiceFactory implementation
// --------------------------------------

ReportingServiceBase* ReportingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ReportingServiceBase*>(
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
