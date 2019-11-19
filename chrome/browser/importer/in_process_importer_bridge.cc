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
#include "services/data_decoder/public/cpp/data_decoder.h"
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
bool FirefoxURLParameterFilter(const std::string& key,
                               const std::string& value) {
  std::string low_value = base::ToLowerASCII(value);
  if (low_value.find("mozilla") != std::string::npos ||
      low_value.find("firefox") != std::string::npos ||
      low_value.find("moz:") != std::string::npos) {
    return false;
  }
  return true;
}

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

}  // namespace

// When the Bridge receives the search engines XML data via
// SetFirefoxSearchEnginesXMLData(), this class is responsible for managing the
// asynchronous TemplateURL parsing operations. The Bridge generally operates
// synchronously, so this class manages the state and notifies the bridge when
// parsing is done.
class InProcessImporterBridge::SearchEnginesParser {
 public:
  // Starts parsing the |search_engines_xml_data| and will notify |bridge|
  // upon completion.
  SearchEnginesParser(const std::vector<std::string>& search_engines_xml_data,
                      InProcessImporterBridge* bridge)
      : bridge_(bridge), data_decoder_(new data_decoder::DataDecoder()) {
    DCHECK(!search_engines_xml_data.empty());
    StartParse(search_engines_xml_data);
  }

  // Returns true if all the data have been parsed, false if the operation
  // is still ongoing.
  bool is_done() const { return is_done_; }

  // If InProcessImporterBridge::NotifyEnded() is called before is_done()
  // returns true, NotifyEnded() sets this flag so that it can be called back
  // to complete the import.
  void set_notify_ended_on_completion() { notify_ended_on_completion_ = true; }

 private:
  void StartParse(const std::vector<std::string>& search_engines_xml_data) {
    const auto& last_item = search_engines_xml_data.end() - 1;
    TemplateURLParser::ParameterFilter param_filter =
        base::BindRepeating(&FirefoxURLParameterFilter);

    for (auto it = search_engines_xml_data.begin();
         it != search_engines_xml_data.end(); ++it) {
      // Because all TemplateURLParser are handled by the same data_decoder_
      // instance, the results will be returned FIFO.
      // The SearchEnginesParser is owned by the InProcessImporterBridge,
      // which is not deleted until NotifyEnded() is called, so using Unretained
      // is safe.
      TemplateURLParser::ParseWithDataDecoder(
          data_decoder_.get(), &search_terms_data_, *it, param_filter,
          base::BindOnce(&SearchEnginesParser::OnURLParsed,
                         base::Unretained(this), it == last_item));
    }
  }

  void OnURLParsed(bool is_last_item, std::unique_ptr<TemplateURL> url) {
    if (url)
      parsed_urls_.push_back(std::move(url));

    if (is_last_item)
      FinishParsing();
  }

  void FinishParsing() {
    is_done_ = true;

    // Shut down the DataDecoder.
    data_decoder_.reset();

    bridge_->WriteSearchEngines(std::move(parsed_urls_));

    if (notify_ended_on_completion_)
      bridge_->NotifyEnded();
  }

  // Storage for the URLs. These are stored in the same order as the original
  // |search_engines_xml_data|.
  TemplateURLService::OwnedTemplateURLVector parsed_urls_;

  InProcessImporterBridge* bridge_;  // Weak, owns this.

  // Set to true if the last search engine has been parsed.
  bool is_done_ = false;

  // Set to true if the ImporterBridge has been NotifyEnded() already but was
  // waiting on this class to finish the import.
  bool notify_ended_on_completion_ = false;

  // Parameter for TemplateURLParser.
  UIThreadSearchTermsData search_terms_data_;

  // The DataDecoder instance that is shared amongst all the TemplateURLs being
  // parsed.
  std::unique_ptr<data_decoder::DataDecoder> data_decoder_;

  DISALLOW_COPY_AND_ASSIGN(SearchEnginesParser);
};

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
  if (!search_engine_data.empty()) {
    // SearchEnginesParser will call back the Bridge back when it is done.
    search_engines_.reset(new SearchEnginesParser(search_engine_data, this));
  }
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
  // If there are search engines to parse but parsing them is not yet complete,
  // arrange to be called back when they are done.
  if (search_engines_ && !search_engines_->is_done()) {
    search_engines_->set_notify_ended_on_completion();
    return;
  }

  host_->NotifyImportEnded();
}

base::string16 InProcessImporterBridge::GetLocalizedString(int message_id) {
  return l10n_util::GetStringUTF16(message_id);
}

InProcessImporterBridge::~InProcessImporterBridge() {}

void InProcessImporterBridge::WriteSearchEngines(
    TemplateURL::OwnedTemplateURLVector template_urls) {
  std::map<std::string, std::unique_ptr<TemplateURL>> search_engine_for_url;
  for (auto& template_url : template_urls) {
    std::string key = template_url->url();
    // Give priority to the latest template URL that is found, as
    // GetSearchEnginesXMLFiles() returns a vector with first Firefox default
    // search engines and then the user's ones. The user ones should take
    // precedence.
    search_engine_for_url[key] = std::move(template_url);
  }
  // The first URL represents the default search engine in Firefox 3, so we
  // need to keep it on top of the list.
  auto default_turl = search_engine_for_url.end();
  if (!template_urls.empty())
    default_turl = search_engine_for_url.find(template_urls[0]->url());

  // Put the results in the |search_engines| vector.
  TemplateURLService::OwnedTemplateURLVector search_engines;
  for (auto it = search_engine_for_url.begin();
       it != search_engine_for_url.end(); ++it) {
    if (it == default_turl) {
      search_engines.insert(search_engines.begin(),
                            std::move(default_turl->second));
    } else {
      search_engines.push_back(std::move(it->second));
    }
  }

  writer_->AddKeywords(std::move(search_engines), true);
}
