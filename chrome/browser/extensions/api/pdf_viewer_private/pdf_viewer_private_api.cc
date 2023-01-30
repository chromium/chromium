// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/pdf_viewer_private/pdf_viewer_private_api.h"

#include "base/values.h"
#include "chrome/browser/pdf/pdf_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "url/url_constants.h"

namespace extensions {

namespace {

namespace IsAllowedLocalFileAccess =
    api::pdf_viewer_private::IsAllowedLocalFileAccess;

namespace SetPdfOcrPref = api::pdf_viewer_private::SetPdfOcrPref;

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

}  // namespace

PdfViewerPrivateIsAllowedLocalFileAccessFunction::
    PdfViewerPrivateIsAllowedLocalFileAccessFunction() = default;

PdfViewerPrivateIsAllowedLocalFileAccessFunction::
    ~PdfViewerPrivateIsAllowedLocalFileAccessFunction() = default;

ExtensionFunction::ResponseAction
PdfViewerPrivateIsAllowedLocalFileAccessFunction::Run() {
  std::unique_ptr<IsAllowedLocalFileAccess::Params> params(
      IsAllowedLocalFileAccess::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  PrefService* prefs =
      Profile::FromBrowserContext(browser_context())->GetPrefs();

  return RespondNow(OneArgument(base::Value(IsUrlAllowedToEmbedLocalFiles(
      GURL(params->url),
      prefs->GetList(prefs::kPdfLocalFileAccessAllowedForDomains)))));
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
  return RespondNow(OneArgument(base::Value(value)));
}

PdfViewerPrivateSetPdfOcrPrefFunction::PdfViewerPrivateSetPdfOcrPrefFunction() =
    default;

PdfViewerPrivateSetPdfOcrPrefFunction::
    ~PdfViewerPrivateSetPdfOcrPrefFunction() = default;

ExtensionFunction::ResponseAction PdfViewerPrivateSetPdfOcrPrefFunction::Run() {
  std::unique_ptr<SetPdfOcrPref::Params> params(
      SetPdfOcrPref::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

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
  return RespondNow(OneArgument(base::Value(true)));
}

}  // namespace extensions
