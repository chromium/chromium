// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/chrome_clipboard_context.h"

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/data_controls/core/browser/prefs.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_service.h"

namespace data_controls {

namespace {

GURL GetURL(const content::ClipboardEndpoint& endpoint) {
  if (!endpoint.data_transfer_endpoint() ||
      !endpoint.data_transfer_endpoint()->IsUrlType() ||
      !endpoint.data_transfer_endpoint()->GetURL()) {
    return GURL();
  }
  return *endpoint.data_transfer_endpoint()->GetURL();
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

ChromeClipboardContext::ChromeClipboardContext(
    content::ClipboardEndpoint source,
    content::ClipboardEndpoint destination,
    ui::ClipboardMetadata metadata)
    : source_(std::move(source)),
      destination_(std::move(destination)),
      metadata_(std::move(metadata)) {}

ChromeClipboardContext::ChromeClipboardContext(
    content::ClipboardEndpoint source,
    ui::ClipboardMetadata metadata)
    : source_(std::move(source)),
      destination_(std::nullopt),
      metadata_(std::move(metadata)) {}

ChromeClipboardContext::~ChromeClipboardContext() = default;

// static
enterprise_connectors::ContentMetaData::CopiedTextSource
ChromeClipboardContext::GetClipboardSource(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const char* scope_pref) {
  CHECK(destination.browser_context());

  using SourceType = enterprise_connectors::ContentMetaData::CopiedTextSource;

  SourceType copied_text_source;
  if (!source.browser_context()) {
    // This off the record check will also include guest profile sources, but
    // since there is no way to disambiguate them with a null BrowserContext
    // INCOGNITO is selected instead of CLIPBOARD to not share the source URL in
    // such cases.
    if (source.data_transfer_endpoint() &&
        source.data_transfer_endpoint()->off_the_record()) {
      copied_text_source.set_context(SourceType::INCOGNITO);
    } else {
      copied_text_source.set_context(SourceType::CLIPBOARD);
    }
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
      break;
    case SourceType::CLIPBOARD:
      // If the user does something like closing the browser between the time
      // they copy and then paste, the DTE might have a URL even though the lack
      // of browser context will make it impossible to know if the `SourceType`
      // is `SAME_PROFILE` or `OTHER_PROFILE`.
      //
      // In that case, we can be conservative and perform the same check as
      // `OTHER_PROFILE`. Note that this code path is unreachable in the case of
      // an incognito source URL as that is handled in the `set_context`
      // conditions above.
      [[fallthrough]];
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

GURL ChromeClipboardContext::source_url() const {
  return GetURL(source_);
}

GURL ChromeClipboardContext::destination_url() const {
  return GetURL(destination_);
}

enterprise_connectors::ContentMetaData::CopiedTextSource
ChromeClipboardContext::data_controls_copied_text_source() const {
  return GetClipboardSource(source_, destination_, kDataControlsRulesScopePref);
}

ui::ClipboardFormatType ChromeClipboardContext::format_type() const {
  return metadata_.format_type;
}

std::optional<size_t> ChromeClipboardContext::size() const {
  return metadata_.size;
}

std::string ChromeClipboardContext::source_active_user() const {
  auto* profile = Profile::FromBrowserContext(source_.browser_context());
  if (!profile) {
    return "";
  }

  return enterprise_connectors::ContentAreaUserProvider::GetUser(
      profile, source_.web_contents(), source_url());
}

std::string ChromeClipboardContext::destination_active_user() const {
  auto* profile = Profile::FromBrowserContext(destination_.browser_context());
  if (!profile) {
    return "";
  }

  return enterprise_connectors::ContentAreaUserProvider::GetUser(
      profile, destination_.web_contents(), destination_url());
}

}  // namespace data_controls
