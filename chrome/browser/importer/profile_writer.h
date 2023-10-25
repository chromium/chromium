// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_PROFILE_WRITER_H_
#define CHROME_BROWSER_IMPORTER_PROFILE_WRITER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "components/history/core/browser/history_types.h"
#include "components/search_engines/template_url_service.h"
#include "url/gurl.h"

struct ImportedBookmarkEntry;
class Profile;

namespace autofill {
class AutocompleteEntry;
}

namespace password_manager {
struct PasswordForm;
}  // namespace password_manager

// ProfileWriter encapsulates profile for writing entries into it.
// This object must be invoked on UI thread.
class ProfileWriter : public base::RefCountedThreadSafe<ProfileWriter> {
 public:
  explicit ProfileWriter(Profile* profile);

  ProfileWriter(const ProfileWriter&) = delete;
  ProfileWriter& operator=(const ProfileWriter&) = delete;

  // These functions return true if the corresponding model has been loaded.
  // If the models haven't been loaded, the importer waits to run until they've
  // completed.
  virtual bool BookmarkModelIsLoaded() const;
  virtual bool TemplateURLServiceIsLoaded() const;

  // Helper methods for adding data to local stores.
  virtual void AddPasswordForm(const password_manager::PasswordForm& form);

  virtual void AddHistoryPage(const history::URLRows& page,
                              history::VisitSource visit_source);

  virtual void AddHomepage(const GURL& homepage);

  // Adds the |bookmarks| to the bookmark model.
  //
  // (a) If the bookmarks bar is empty:
  //     (i) If |bookmarks| includes at least one bookmark that was originally
  //         located in a toolbar, all such bookmarks are imported directly to
  //         the toolbar; any other bookmarks are imported to a subfolder in
  //         the toolbar.
  //     (i) If |bookmarks| includes no bookmarks that were originally located
  //         in a toolbar, all bookmarks are imported directly to the toolbar.
  // (b) If the bookmarks bar is not empty, all bookmarks are imported to a
  //     subfolder in the toolbar.
  //
  // In either case, if a subfolder is created, the name will be the value of
  // |top_level_folder_name|, unless a folder with this name already exists.
  // If a folder with this name already exists, then the name is uniquified.
  // For example, if |first_folder_name| is 'Imported from IE' and a folder with
  // the name 'Imported from IE' already exists in the bookmarks toolbar, then
  // we will instead create a subfolder named 'Imported from IE (1)'.
  virtual void AddBookmarks(const std::vector<ImportedBookmarkEntry>& bookmarks,
                            const std::u16string& top_level_folder_name);

  virtual void AddFavicons(const favicon_base::FaviconUsageDataList& favicons);

  // Adds the TemplateURLs in |template_urls| to the local store.
  // Some TemplateURLs in |template_urls| may conflict (same keyword or same
  // host name in the URL) with existing TemplateURLs in the local store, in
  // which case the existing ones take precedence and the duplicates in
  // |template_urls| are deleted. If |unique_on_host_and_path| is true, a
  // TemplateURL is only added if there is not an existing TemplateURL that has
  // a replaceable search url with the same host+path combination.
  virtual void AddKeywords(
      TemplateURLService::OwnedTemplateURLVector template_urls,
      bool unique_on_host_and_path);

  // Adds the imported autocomplete entries to the autofill database.
  virtual void AddAutocompleteFormDataEntries(
      const std::vector<autofill::AutocompleteEntry>& autocomplete_entries);

 protected:
  friend class base::RefCountedThreadSafe<ProfileWriter>;

  virtual ~ProfileWriter();

 private:
  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_IMPORTER_PROFILE_WRITER_H_
