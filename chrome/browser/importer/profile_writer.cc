// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/profile_writer.h"

#include <stddef.h>

#include <map>
#include <set>
#include <string>

#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/favicon/core/favicon_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

// Generates a unique folder name. If |folder_name| is not unique, then this
// repeatedly tests for '|folder_name| + (i)' until a unique name is found.
std::u16string GenerateUniqueFolderName(BookmarkModel* model,
                                        const std::u16string& folder_name) {
  // Build a set containing the bookmark bar folder names.
  std::set<std::u16string> existing_folder_names;
  const BookmarkNode* bookmark_bar = model->bookmark_bar_node();
  for (const auto& node : bookmark_bar->children()) {
    if (node->is_folder())
      existing_folder_names.insert(node->GetTitle());
  }

  // If the given name is unique, use it.
  if (existing_folder_names.find(folder_name) == existing_folder_names.end())
    return folder_name;

  // Otherwise iterate until we find a unique name.
  for (size_t i = 1; i <= existing_folder_names.size(); ++i) {
    std::u16string name =
        folder_name + u" (" + base::NumberToString16(i) + u")";
    if (existing_folder_names.find(name) == existing_folder_names.end())
      return name;
  }

  NOTREACHED_IN_MIGRATION();
  return folder_name;
}

// Shows the bookmarks toolbar.
void ShowBookmarkBar(Profile* profile) {
  profile->GetPrefs()->SetBoolean(bookmarks::prefs::kShowBookmarkBar, true);
}

}  // namespace

ProfileWriter::ProfileWriter(Profile* profile) : profile_(profile) {}

bool ProfileWriter::BookmarkModelIsLoaded() const {
  return BookmarkModelFactory::GetForBrowserContext(profile_)->loaded();
}

bool ProfileWriter::TemplateURLServiceIsLoaded() const {
  return TemplateURLServiceFactory::GetForProfile(profile_)->loaded();
}

void ProfileWriter::AddPasswordForm(
    const password_manager::PasswordForm& form) {
  DCHECK(profile_);

  if (profile_->GetPrefs()->GetBoolean(
          password_manager::prefs::kCredentialsEnableService)) {
    ProfilePasswordStoreFactory::GetForProfile(
        profile_, ServiceAccessType::EXPLICIT_ACCESS)
        ->AddLogin(form);
  }
}

void ProfileWriter::AddHistoryPage(const history::URLRows& page,
                                   history::VisitSource visit_source) {
  if (!page.empty()) {
    HistoryServiceFactory::GetForProfile(profile_,
                                         ServiceAccessType::EXPLICIT_ACCESS)
        ->AddPagesWithDetails(page, visit_source);
  }
}

void ProfileWriter::AddHomepage(const GURL& home_page) {
  DCHECK(profile_);

  PrefService* prefs = profile_->GetPrefs();
  // NOTE: We set the kHomePage value, but keep the NewTab page as the homepage.
  const PrefService::Preference* pref = prefs->FindPreference(prefs::kHomePage);
  if (pref && !pref->IsManaged()) {
    prefs->SetString(prefs::kHomePage, home_page.spec());
  }
}

void ProfileWriter::AddBookmarks(
    const std::vector<ImportedBookmarkEntry>& bookmarks,
    const std::u16string& top_level_folder_name) {
  if (bookmarks.empty())
    return;

  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile_);
  DCHECK(model->loaded());

  // If the bookmark bar is currently empty, we should import directly to it.
  // Otherwise, we should import everything to a subfolder.
  const BookmarkNode* bookmark_bar = model->bookmark_bar_node();
  bool import_to_top_level = bookmark_bar->children().empty();

  // Reorder bookmarks so that the toolbar entries come first.
  std::vector<ImportedBookmarkEntry> toolbar_bookmarks;
  std::vector<ImportedBookmarkEntry> reordered_bookmarks;
  for (auto it = bookmarks.begin(); it != bookmarks.end(); ++it) {
    if (it->in_toolbar)
      toolbar_bookmarks.push_back(*it);
    else
      reordered_bookmarks.push_back(*it);
  }
  reordered_bookmarks.insert(reordered_bookmarks.begin(),
                             toolbar_bookmarks.begin(),
                             toolbar_bookmarks.end());

  // If the user currently has no bookmarks in the bookmark bar, make sure that
  // at least some of the imported bookmarks end up there.  Otherwise, we'll end
  // up with just a single folder containing the imported bookmarks, which makes
  // for unnecessary nesting.
  bool add_all_to_top_level = import_to_top_level && toolbar_bookmarks.empty();

  model->BeginExtensiveChanges();

  std::set<const BookmarkNode*> folders_added_to;
  const BookmarkNode* top_level_folder = nullptr;
  for (std::vector<ImportedBookmarkEntry>::const_iterator bookmark =
           reordered_bookmarks.begin();
       bookmark != reordered_bookmarks.end(); ++bookmark) {
    // Disregard any bookmarks with invalid urls.
    if (!bookmark->is_folder && !bookmark->url.is_valid())
      continue;

    const BookmarkNode* parent = nullptr;
    if (import_to_top_level && (add_all_to_top_level || bookmark->in_toolbar)) {
      // Add directly to the bookmarks bar.
      parent = bookmark_bar;
    } else {
      // Add to a folder that will contain all the imported bookmarks not added
      // to the bar.  The first time we do so, create the folder.
      if (!top_level_folder) {
        std::u16string name =
            GenerateUniqueFolderName(model, top_level_folder_name);
        top_level_folder = model->AddFolder(
            bookmark_bar, bookmark_bar->children().size(), name);
      }
      parent = top_level_folder;
    }

    // Ensure any enclosing folders are present in the model.  The bookmark's
    // enclosing folder structure should be
    //   path[0] > path[1] > ... > path[size() - 1]
    for (auto folder_name = bookmark->path.begin();
         folder_name != bookmark->path.end(); ++folder_name) {
      if (bookmark->in_toolbar && parent == bookmark_bar &&
          folder_name == bookmark->path.begin()) {
        // If we're importing directly to the bookmarks bar, skip over the
        // folder named "Bookmarks Toolbar" (or any non-Firefox equivalent).
        continue;
      }

      const auto it = base::ranges::find_if(
          parent->children(), [folder_name](const auto& node) {
            return node->is_folder() && node->GetTitle() == *folder_name;
          });
      parent = (it == parent->children().cend())
                   ? model->AddFolder(parent, parent->children().size(),
                                      *folder_name)
                   : it->get();
    }

    folders_added_to.insert(parent);
    if (bookmark->is_folder) {
      model->AddFolder(parent, parent->children().size(), bookmark->title);
    } else {
      model->AddURL(parent, parent->children().size(), bookmark->title,
                    bookmark->url, nullptr, bookmark->creation_time);
    }
  }

  // In order to keep the imported-to folders from appearing in the 'recently
  // added to' combobox, reset their modified times.
  for (auto i = folders_added_to.begin(); i != folders_added_to.end(); ++i) {
    model->ResetDateFolderModified(*i);
  }

  model->EndExtensiveChanges();

  // If the user was previously using a toolbar, we should show the bar.
  if (import_to_top_level && !add_all_to_top_level)
    ShowBookmarkBar(profile_);
}

void ProfileWriter::AddFavicons(
    const favicon_base::FaviconUsageDataList& favicons) {
  FaviconServiceFactory::GetForProfile(profile_,
                                       ServiceAccessType::EXPLICIT_ACCESS)
      ->SetImportedFavicons(favicons);
}

typedef std::map<std::string, TemplateURL*> HostPathMap;

// Returns the key for the map built by BuildHostPathMap. If url_string is not
// a valid URL, an empty string is returned, otherwise host+path is returned.
static std::string HostPathKeyForURL(const GURL& url) {
  return url.is_valid() ? url.host() + url.path() : std::string();
}

// Builds the key to use in HostPathMap for the specified TemplateURL. Returns
// an empty string if a host+path can't be generated for the TemplateURL.
// If an empty string is returned, the TemplateURL should not be added to
// HostPathMap.
//
// If |try_url_if_invalid| is true, and |t_url| isn't valid, a string is built
// from the raw TemplateURL string. Use a value of true for |try_url_if_invalid|
// when checking imported URLs as the imported URL may not be valid yet may
// match the host+path of one of the default URLs. This is used to catch the
// case of IE using an invalid OSDD URL for Live Search, yet the host+path
// matches our prepopulate data. IE's URL for Live Search is something like
// 'http://...{Language}...'. As {Language} is not a valid OSDD parameter value
// the TemplateURL is invalid.
static std::string BuildHostPathKey(const TemplateURL* t_url,
                                    const SearchTermsData& search_terms_data,
                                    bool try_url_if_invalid) {
  if (try_url_if_invalid && !t_url->url_ref().IsValid(search_terms_data))
    return HostPathKeyForURL(GURL(t_url->url()));

  if (t_url->url_ref().SupportsReplacement(search_terms_data)) {
    return HostPathKeyForURL(GURL(t_url->url_ref().ReplaceSearchTerms(
        TemplateURLRef::SearchTermsArgs(u"x"), search_terms_data)));
  }
  return std::string();
}

// Builds a set that contains an entry of the host+path for each TemplateURL in
// the TemplateURLService that has a valid search url.
static void BuildHostPathMap(TemplateURLService* model,
                             HostPathMap* host_path_map) {
  TemplateURLService::TemplateURLVector template_urls =
      model->GetTemplateURLs();
  for (size_t i = 0; i < template_urls.size(); ++i) {
    const std::string host_path = BuildHostPathKey(
        template_urls[i], model->search_terms_data(), false);
    if (!host_path.empty()) {
      const TemplateURL* existing_turl = (*host_path_map)[host_path];
      TemplateURL* t_url = template_urls[i];
      if (!existing_turl || (model->ShowInDefaultList(t_url) &&
                             !model->ShowInDefaultList(existing_turl))) {
        // If there are multiple TemplateURLs with the same host+path, favor
        // those shown in the default list.  If there are multiple potential
        // defaults, favor the first one, which should be the more commonly used
        // one.
        (*host_path_map)[host_path] = t_url;
      }
    }  // else case, TemplateURL doesn't have a search url, doesn't support
       // replacement, or doesn't have valid GURL. Ignore it.
  }
}

void ProfileWriter::AddKeywords(
    TemplateURLService::OwnedTemplateURLVector template_urls,
    bool unique_on_host_and_path) {
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(profile_);
  HostPathMap host_path_map;
  if (unique_on_host_and_path)
    BuildHostPathMap(model, &host_path_map);

  for (auto& turl : template_urls) {
    // TemplateURLService requires keywords to be unique. If there is already a
    // TemplateURL with this keyword, don't import it again.
    if (model->GetTemplateURLForKeyword(turl->keyword()) != nullptr)
      continue;

    // The omnibox doesn't properly handle search keywords with whitespace,
    // so skip importing them.
    if (turl->keyword().find_first_of(base::kWhitespaceUTF16) !=
        std::u16string::npos)
      continue;

    // For search engines if there is already a keyword with the same
    // host+path, we don't import it. This is done to avoid both duplicate
    // search providers (such as two Googles, or two Yahoos) as well as making
    // sure the search engines we provide aren't replaced by those from the
    // imported browser.
    if (unique_on_host_and_path && (host_path_map.find(BuildHostPathKey(
                                        turl.get(), model->search_terms_data(),
                                        true)) != host_path_map.end()))
      continue;

    // Only add valid TemplateURLs to the model.
    if (turl->url_ref().IsValid(model->search_terms_data()))
      model->Add(std::move(turl));
  }
}

void ProfileWriter::AddAutocompleteFormDataEntries(
    const std::vector<autofill::AutocompleteEntry>& autocomplete_entries) {
  scoped_refptr<autofill::AutofillWebDataService> web_data_service =
      WebDataServiceFactory::GetAutofillWebDataForProfile(
          profile_, ServiceAccessType::EXPLICIT_ACCESS);
  if (web_data_service.get())
    web_data_service->UpdateAutocompleteEntries(autocomplete_entries);
}

ProfileWriter::~ProfileWriter() {}
