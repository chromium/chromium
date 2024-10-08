// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_action_handler.h"

#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/scanner/scanner_action.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "base/strings/escape.h"
#include "url/gurl.h"

namespace ash {

namespace {

const GURL& GetCalendarEventTemplateUrl() {
  // Required to delay the creation of this GURL to avoid hitting the
  // `url::DoSchemeModificationPreamble` DCHECK.
  static GURL kGoogleCalendarEventTemplateUrl(
      "https://calendar.google.com/calendar/render?action=TEMPLATE");
  return kGoogleCalendarEventTemplateUrl;
}

const GURL& GetGoogleContactsNewUrl() {
  static GURL kGoogleContactsNewUrl("https://contacts.google.com/new");
  return kGoogleContactsNewUrl;
}

GURL GetCalendarEventUrl(const NewCalendarEventAction& event) {
  std::string query = GetCalendarEventTemplateUrl().query();
  CHECK(!query.empty());
  if (!event.title.empty()) {
    query += "&text=";
    query += base::EscapeQueryParamValue(event.title, /*use_plus=*/true);
  }

  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  return GetCalendarEventTemplateUrl().ReplaceComponents(replacements);
}

GURL GetContactUrl(const NewContactAction& contact) {
  std::string query = GetGoogleContactsNewUrl().query();
  CHECK(query.empty());
  if (!contact.given_name.empty()) {
    query += "given_name=";
    query += base::EscapeQueryParamValue(contact.given_name, /*use_plus=*/true);
  }

  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  return GetGoogleContactsNewUrl().ReplaceComponents(replacements);
}

void OpenInBrowserTab(const GURL& gurl) {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      gurl, NewWindowDelegate::OpenUrlFrom::kUnspecified,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

}  // namespace

void HandleScannerAction(const ScannerAction& action,
                         base::OnceCallback<void(bool)> callback) {
  std::visit(base::Overloaded{
                 [&](const NewCalendarEventAction& action) {
                   OpenInBrowserTab(GetCalendarEventUrl(action));
                   std::move(callback).Run(true);
                 },
                 [&](const NewContactAction& action) {
                   OpenInBrowserTab(GetContactUrl(action));
                   std::move(callback).Run(true);
                 },
             },
             action);
}

}  // namespace ash
