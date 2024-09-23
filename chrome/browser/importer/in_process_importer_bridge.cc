// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/in_process_importer_bridge.h"

#include <stddef.h>

#include <iterator>

#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/importer/external_process_importer_host.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/importer_autofill_form_data_entry.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_parser.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

history::URLRows ConvertImporterURLRowsToHistoryURLRows(
    const std::vector<ImporterURLRow>& rows) {
  history::URLRows converted;
  converted.reserve(rows.size());
  for (auto it = rows.begin(); it != rows.end(); ++it) {
    history::URLRow row(it->url);
    row.set_title(it->title);
    row.set_visit_count(it->visit_count);
    row.set_typed_count(it->typed_count);
    row.set_last_visit(it->last_visit);
    row.set_hidden(it->hidden);
    converted.push_back(row);
  }
  return converted;
}

history::VisitSource ConvertImporterVisitSourceToHistoryVisitSource(
    importer::VisitSource visit_source) {
  switch (visit_source) {
    case importer::VISIT_SOURCE_BROWSED:
      return history::SOURCE_BROWSED;
    case importer::VISIT_SOURCE_FIREFOX_IMPORTED:
      return history::SOURCE_FIREFOX_IMPORTED;
    case importer::VISIT_SOURCE_IE_IMPORTED:
      return history::SOURCE_IE_IMPORTED;
    case importer::VISIT_SOURCE_SAFARI_IMPORTED:
      return history::SOURCE_SAFARI_IMPORTED;
  }
  NOTREACHED_IN_MIGRATION();
  return history::SOURCE_SYNCED;
}

password_manager::PasswordForm::Scheme ConvertToPasswordFormScheme(
    importer::ImportedPasswordForm::Scheme scheme) {
  switch (scheme) {
    case importer::ImportedPasswordForm::Scheme::kHtml:
      return password_manager::PasswordForm::Scheme::kHtml;
    case importer::ImportedPasswordForm::Scheme::kBasic:
      return password_manager::PasswordForm::Scheme::kBasic;
  }

  NOTREACHED_IN_MIGRATION();
  return password_manager::PasswordForm::Scheme::kHtml;
}

password_manager::PasswordForm ConvertImportedPasswordForm(
    const importer::ImportedPasswordForm& form) {
  password_manager::PasswordForm result;
  result.scheme = ConvertToPasswordFormScheme(form.scheme);
  result.signon_realm = form.signon_realm;
  result.url = form.url;
  result.action = form.action;
  result.username_element = form.username_element;
  result.username_value = form.username_value;
  result.password_element = form.password_element;
  result.password_value = form.password_value;
  result.blocked_by_user = form.blocked_by_user;
  return result;
}

// Attempts to create a TemplateURL from the provided data. |title| is optional.
// If TemplateURL creation fails, returns null.
std::unique_ptr<TemplateURL> CreateTemplateURL(const std::u16string& url,
                                               const std::u16string& keyword,
                                               const std::u16string& title) {
  if (url.empty() || keyword.empty())
    return nullptr;
  TemplateURLData data;
  data.SetKeyword(keyword);
  // We set short name by using the title if it exists.
  // Otherwise, we use the shortcut.
  data.SetShortName(title.empty() ? keyword : title);
  data.SetURL(TemplateURLRef::DisplayURLToURLRef(url));
  return std::make_unique<TemplateURL>(data);
}

}  // namespace

InProcessImporterBridge::InProcessImporterBridge(
    ProfileWriter* writer,
    base::WeakPtr<ExternalProcessImporterHost> host) : writer_(writer),
                                                       host_(host) {
}

void InProcessImporterBridge::AddBookmarks(
    const std::vector<ImportedBookmarkEntry>& bookmarks,
    const std::u16string& first_folder_name) {
  writer_->AddBookmarks(bookmarks, first_folder_name);
}

void InProcessImporterBridge::AddHomePage(const GURL& home_page) {
  writer_->AddHomepage(home_page);
}

void InProcessImporterBridge::SetFavicons(
    const favicon_base::FaviconUsageDataList& favicons) {
  writer_->AddFavicons(favicons);
}

void InProcessImporterBridge::SetHistoryItems(
    const std::vector<ImporterURLRow>& rows,
    importer::VisitSource visit_source) {
  history::URLRows converted_rows =
      ConvertImporterURLRowsToHistoryURLRows(rows);
  history::VisitSource converted_visit_source =
      ConvertImporterVisitSourceToHistoryVisitSource(visit_source);
  writer_->AddHistoryPage(converted_rows, converted_visit_source);
}

void InProcessImporterBridge::SetKeywords(
    const std::vector<importer::SearchEngineInfo>& search_engines,
    bool unique_on_host_and_path) {
  TemplateURLService::OwnedTemplateURLVector owned_template_urls;
  for (const auto& search_engine : search_engines) {
    std::unique_ptr<TemplateURL> owned_template_url = CreateTemplateURL(
        search_engine.url, search_engine.keyword, search_engine.display_name);
    if (owned_template_url)
      owned_template_urls.push_back(std::move(owned_template_url));
  }
  writer_->AddKeywords(std::move(owned_template_urls), unique_on_host_and_path);
}

void InProcessImporterBridge::SetPasswordForm(
    const importer::ImportedPasswordForm& form) {
  writer_->AddPasswordForm(ConvertImportedPasswordForm(form));
}

void InProcessImporterBridge::SetAutofillFormData(
    const std::vector<ImporterAutofillFormDataEntry>& entries) {
  std::vector<autofill::AutocompleteEntry> autocomplete_entries;
  for (const ImporterAutofillFormDataEntry& entry : entries) {
    // Using method c_str() in order to avoid data which contains null
    // terminating symbols.
    const std::u16string name = entry.name.c_str();
    const std::u16string value = entry.value.c_str();
    if (name.empty() || value.empty())
      continue;
    autocomplete_entries.emplace_back(autofill::AutocompleteKey(name, value),
                                      entry.first_used, entry.last_used);
  }

  writer_->AddAutocompleteFormDataEntries(autocomplete_entries);
}

void InProcessImporterBridge::NotifyStarted() {
  host_->NotifyImportStarted();
}

void InProcessImporterBridge::NotifyItemStarted(importer::ImportItem item) {
  host_->NotifyImportItemStarted(item);
}

void InProcessImporterBridge::NotifyItemEnded(importer::ImportItem item) {
  host_->NotifyImportItemEnded(item);
}

void InProcessImporterBridge::NotifyEnded() {
  host_->NotifyImportEnded();
}

std::u16string InProcessImporterBridge::GetLocalizedString(int message_id) {
  return l10n_util::GetStringUTF16(message_id);
}

InProcessImporterBridge::~InProcessImporterBridge() {}
