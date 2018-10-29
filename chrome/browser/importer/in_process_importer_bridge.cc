// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/in_process_importer_bridge.h"

#include <stddef.h>

#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/importer/external_process_importer_host.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/importer_autofill_form_data_entry.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/common/password_form.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_parser.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "ui/base/l10n/l10n_util.h"

#include <iterator>

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
  NOTREACHED();
  return history::SOURCE_SYNCED;
}

}  // namespace

namespace {

// FirefoxURLParameterFilter is used to remove parameter mentioning Firefox from
// the search URL when importing search engines.
class FirefoxURLParameterFilter : public TemplateURLParser::ParameterFilter {
 public:
  FirefoxURLParameterFilter() {}
  ~FirefoxURLParameterFilter() override {}

  // TemplateURLParser::ParameterFilter method.
  bool KeepParameter(const std::string& key,
                     const std::string& value) override {
    std::string low_value = base::ToLowerASCII(value);
    if (low_value.find("mozilla") != std::string::npos ||
        low_value.find("firefox") != std::string::npos ||
        low_value.find("moz:") != std::string::npos) {
      return false;
    }
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FirefoxURLParameterFilter);
};

// Attempts to create a TemplateURL from the provided data. |title| is optional.
// If TemplateURL creation fails, returns null.
std::unique_ptr<TemplateURL> CreateTemplateURL(const base::string16& url,
                                               const base::string16& keyword,
                                               const base::string16& title) {
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

// Parses the OpenSearch XML files in |xml_files| and populates |search_engines|
// with the resulting TemplateURLs.
void ParseSearchEnginesFromFirefoxXMLData(
    const std::vector<std::string>& xml_data,
    TemplateURLService::OwnedTemplateURLVector* search_engines) {
  DCHECK(search_engines);

  std::map<std::string, std::unique_ptr<TemplateURL>> search_engine_for_url;
  FirefoxURLParameterFilter param_filter;
  // The first XML file represents the default search engine in Firefox 3, so we
  // need to keep it on top of the list.
  auto default_turl = search_engine_for_url.end();
  for (auto xml_iter = xml_data.begin(); xml_iter != xml_data.end();
       ++xml_iter) {
    std::unique_ptr<TemplateURL> template_url = TemplateURLParser::Parse(
        UIThreadSearchTermsData(nullptr), xml_iter->data(), xml_iter->length(),
        &param_filter);
    if (template_url) {
      auto iter = search_engine_for_url.find(template_url->url());
      if (iter == search_engine_for_url.end()) {
        iter = search_engine_for_url
                   .insert(std::make_pair(template_url->url(),
                                          std::move(template_url)))
                   .first;
      } else {
        // We have already found a search engine with the same URL.  We give
        // priority to the latest one found, as GetSearchEnginesXMLFiles()
        // returns a vector with first Firefox default search engines and then
        // the user's ones.  We want to give priority to the user ones.
        iter->second = std::move(template_url);
      }
      if (default_turl == search_engine_for_url.end())
        default_turl = iter;
    }
  }

  // Put the results in the |search_engines| vector.
  for (auto t_iter = search_engine_for_url.begin();
       t_iter != search_engine_for_url.end(); ++t_iter) {
    if (t_iter == default_turl)
      search_engines->insert(search_engines->begin(),
                             std::move(default_turl->second));
    else
      search_engines->push_back(std::move(t_iter->second));
  }
}

}  // namespace

InProcessImporterBridge::InProcessImporterBridge(
    ProfileWriter* writer,
    base::WeakPtr<ExternalProcessImporterHost> host) : writer_(writer),
                                                       host_(host) {
}

void InProcessImporterBridge::AddBookmarks(
    const std::vector<ImportedBookmarkEntry>& bookmarks,
    const base::string16& first_folder_name) {
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

void InProcessImporterBridge::SetFirefoxSearchEnginesXMLData(
    const std::vector<std::string>& search_engine_data) {
  TemplateURLService::OwnedTemplateURLVector search_engines;
  ParseSearchEnginesFromFirefoxXMLData(search_engine_data, &search_engines);

  writer_->AddKeywords(std::move(search_engines), true);
}

void InProcessImporterBridge::SetPasswordForm(
    const autofill::PasswordForm& form) {
  writer_->AddPasswordForm(form);
}

void InProcessImporterBridge::SetAutofillFormData(
    const std::vector<ImporterAutofillFormDataEntry>& entries) {
  std::vector<autofill::AutofillEntry> autofill_entries;
  for (size_t i = 0; i < entries.size(); ++i) {
    // Using method c_str() in order to avoid data which contains null
    // terminating symbols.
    const base::string16 name = entries[i].name.c_str();
    const base::string16 value = entries[i].value.c_str();
    if (name.empty() || value.empty())
      continue;
    autofill_entries.push_back(
        autofill::AutofillEntry(autofill::AutofillKey(name, value),
                                entries[i].first_used, entries[i].last_used));
  }

  writer_->AddAutofillFormDataEntries(autofill_entries);
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

base::string16 InProcessImporterBridge::GetLocalizedString(int message_id) {
  return l10n_util::GetStringUTF16(message_id);
}

InProcessImporterBridge::~InProcessImporterBridge() {}
