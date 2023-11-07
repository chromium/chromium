// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/pdf_viewer_private/pdf_viewer_private_api.h"

#include <cmath>

#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/values.h"
#include "chrome/browser/pdf/pdf_pref_names.h"
#include "chrome/browser/pdf/pdf_viewer_stream_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "url/url_constants.h"

namespace extensions {

namespace {

namespace IsAllowedLocalFileAccess =
    api::pdf_viewer_private::IsAllowedLocalFileAccess;

namespace SetPdfOcrPref = api::pdf_viewer_private::SetPdfOcrPref;

namespace SetPdfPluginAttributes =
    api::pdf_viewer_private::SetPdfPluginAttributes;

// Check if the current URL is allowed based on a list of allowlisted domains.
bool IsUrlAllowedToEmbedLocalFiles(
    const GURL& current_url,
    const base::Value::List& allowlisted_domains) {
  if (!current_url.is_valid() || !current_url.SchemeIs(url::kHttpsScheme)) {
    return false;
  }

  for (auto const& value : allowlisted_domains) {
    const std::string* domain = value.GetIfString();
    if (!domain) {
      continue;
    }

    if (current_url.DomainIs(*domain)) {
      return true;
    }
  }
  return false;
}

// Get the `StreamContainer` associated with the `extension_host`.
base::WeakPtr<StreamContainer> GetStreamContainer(
    content::RenderFrameHost* extension_host) {
  content::RenderFrameHost* embedder_host = extension_host->GetParent();
  if (!embedder_host) {
    return nullptr;
  }

  auto* pdf_viewer_stream_manager =
      pdf::PdfViewerStreamManager::FromWebContents(
          content::WebContents::FromRenderFrameHost(embedder_host));
  if (!pdf_viewer_stream_manager) {
    return nullptr;
  }

  return pdf_viewer_stream_manager->GetStreamContainer(embedder_host);
}

}  // namespace

PdfViewerPrivateGetStreamInfoFunction::PdfViewerPrivateGetStreamInfoFunction() =
    default;

PdfViewerPrivateGetStreamInfoFunction::
    ~PdfViewerPrivateGetStreamInfoFunction() = default;

ExtensionFunction::ResponseAction PdfViewerPrivateGetStreamInfoFunction::Run() {
  base::WeakPtr<StreamContainer> stream =
      GetStreamContainer(render_frame_host());
  if (!stream) {
    return RespondNow(Error("Failed to get StreamContainer"));
  }

  api::pdf_viewer_private::StreamInfo stream_info;
  stream_info.original_url = stream->original_url().spec();
  stream_info.stream_url = stream->stream_url().spec();
  stream_info.tab_id = stream->tab_id();
  stream_info.embedded = stream->embedded();
  return RespondNow(WithArguments(stream_info.ToValue()));
}

PdfViewerPrivateIsAllowedLocalFileAccessFunction::
    PdfViewerPrivateIsAllowedLocalFileAccessFunction() = default;

PdfViewerPrivateIsAllowedLocalFileAccessFunction::
    ~PdfViewerPrivateIsAllowedLocalFileAccessFunction() = default;

ExtensionFunction::ResponseAction
PdfViewerPrivateIsAllowedLocalFileAccessFunction::Run() {
  absl::optional<IsAllowedLocalFileAccess::Params> params =
      IsAllowedLocalFileAccess::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  PrefService* prefs =
      Profile::FromBrowserContext(browser_context())->GetPrefs();

  return RespondNow(WithArguments(IsUrlAllowedToEmbedLocalFiles(
      GURL(params->url),
      prefs->GetList(prefs::kPdfLocalFileAccessAllowedForDomains))));
}

PdfViewerPrivateIsPdfOcrAlwaysActiveFunction::
    PdfViewerPrivateIsPdfOcrAlwaysActiveFunction() = default;

PdfViewerPrivateIsPdfOcrAlwaysActiveFunction::
    ~PdfViewerPrivateIsPdfOcrAlwaysActiveFunction() = default;

ExtensionFunction::ResponseAction
PdfViewerPrivateIsPdfOcrAlwaysActiveFunction::Run() {
  PrefService* prefs =
      Profile::FromBrowserContext(browser_context())->GetPrefs();
  DCHECK(prefs);

  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kAccessibilityPdfOcrAlwaysActive);
  if (!pref) {
    return RespondNow(
        Error("Pref not found: *", prefs::kAccessibilityPdfOcrAlwaysActive));
  }

  DCHECK(pref->GetValue()->is_bool());
  bool value = pref->GetValue()->GetBool();
  return RespondNow(WithArguments(value));
}

PdfViewerPrivateSetPdfOcrPrefFunction::PdfViewerPrivateSetPdfOcrPrefFunction() =
    default;

PdfViewerPrivateSetPdfOcrPrefFunction::
    ~PdfViewerPrivateSetPdfOcrPrefFunction() = default;

ExtensionFunction::ResponseAction PdfViewerPrivateSetPdfOcrPrefFunction::Run() {
  absl::optional<SetPdfOcrPref::Params> params =
      SetPdfOcrPref::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  PrefService* prefs =
      Profile::FromBrowserContext(browser_context())->GetPrefs();
  DCHECK(prefs);

  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kAccessibilityPdfOcrAlwaysActive);
  if (!pref) {
    return RespondNow(
        Error("Pref not found: *", prefs::kAccessibilityPdfOcrAlwaysActive));
  }

  DCHECK(pref->GetValue()->is_bool());
  prefs->SetBoolean(prefs::kAccessibilityPdfOcrAlwaysActive, params->value);
  return RespondNow(WithArguments(true));
}

PdfViewerPrivateSetPdfPluginAttributesFunction::
    PdfViewerPrivateSetPdfPluginAttributesFunction() = default;

PdfViewerPrivateSetPdfPluginAttributesFunction::
    ~PdfViewerPrivateSetPdfPluginAttributesFunction() = default;

ExtensionFunction::ResponseAction
PdfViewerPrivateSetPdfPluginAttributesFunction::Run() {
  absl::optional<SetPdfPluginAttributes::Params> params =
      SetPdfPluginAttributes::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  base::WeakPtr<StreamContainer> stream =
      GetStreamContainer(render_frame_host());
  if (!stream) {
    return RespondNow(Error("Failed to get StreamContainer"));
  }

  const api::pdf_viewer_private::PdfPluginAttributes& attributes =
      params->attributes;
  // Check the `background_color` is an integer.
  double whole = 0.0;
  if (std::modf(attributes.background_color, &whole) != 0.0) {
    return RespondNow(Error("Background color is not an integer"));
  }

  // Check the `background_color` is within the range of a uint32_t.
  if (!base::IsValueInRangeForNumericType<uint32_t>(
          attributes.background_color)) {
    return RespondNow(Error("Background color out of bounds"));
  }

  stream->set_pdf_plugin_attributes(mime_handler::PdfPluginAttributes::New(
      /*background_color=*/attributes.background_color,
      /*allow_javascript=*/attributes.allow_javascript));
  return RespondNow(NoArguments());
}

}  // namespace extensions
