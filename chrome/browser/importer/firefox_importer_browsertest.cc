// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/importer/external_process_importer_host.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "chrome/browser/importer/importer_unittest_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/search_engines/template_url.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(estade): some of these are disabled on mac. http://crbug.com/48007
// TODO(jschuh): Disabled on Win64 build. http://crbug.com/179688
#if defined(OS_MAC) || (defined(OS_WIN) && defined(ARCH_CPU_X86_64))
#define MAYBE_IMPORTER(x) DISABLED_##x
#else
#define MAYBE_IMPORTER(x) x
#endif

// TODO(kszatan): Disabled all tests on old profiles. http://crbug.com/592239
#undef MAYBE_IMPORTER
#define MAYBE_IMPORTER(x) DISABLED_##x

namespace {

struct PasswordInfo {
  const char* origin;
  const char* action;
  const char* realm;
  const char* username_element;
  const char* username;
  const char* password_element;
  const char* password;
  bool blacklisted;
};

struct KeywordInfo {
  const wchar_t* keyword_in_sqlite;
  const wchar_t* keyword_in_json;
  const char* url;
};

struct AutofillFormDataInfo {
  const char* name;
  const char* value;
};

const BookmarkInfo kFirefoxBookmarks[] = {
  {true, 1, {"Bookmarks Toolbar"},
    L"Toolbar",
    "http://site/"},
  {false, 0, {},
    L"Title",
    "http://www.google.com/"},
};

const PasswordInfo kFirefoxPasswords[] = {
  {"http://blacklist.com/", "", "http://blacklist.com/",
      "", "", "", "", true},
  {"http://localhost:8080/", "http://localhost:8080/", "http://localhost:8080/",
    "loginuser", "abc", "loginpass", "123", false},
  {"http://localhost:8080/", "", "http://localhost:8080/localhost",
      "", "http", "", "Http1+1abcdefg", false},
  {"http://server.com:1234/", "", "http://server.com:1234/http_realm",
      "loginuser", "user", "loginpass", "password", false},
  {"http://server.com:4321/", "", "http://server.com:4321/http_realm",
      "loginuser", "user", "loginpass", "", false},
  {"http://server.com:4321/", "", "http://server.com:4321/http_realm",
      "loginuser", "", "loginpass", "password", false},
};

const KeywordInfo kFirefoxKeywords[] = {
    {L"amazon.com", L"amazon.com",
     "http://www.amazon.com/exec/obidos/external-search/?field-keywords="
     "{searchTerms}&mode=blended"},
    {L"answers.com", L"answers.com",
     "http://www.answers.com/main/ntquery?s={searchTerms}&gwp=13"},
    {L"search.creativecommons.org", L"search.creativecommons.org",
     "http://search.creativecommons.org/?q={searchTerms}"},
    {L"search.ebay.com", L"search.ebay.com",
     "http://search.ebay.com/search/search.dll?query={searchTerms}&"
     "MfcISAPICommand=GetResult&ht=1&ebaytag1=ebayreg&srchdesc=n&"
     "maxRecordsReturned=300&maxRecordsPerPage=50&SortProperty=MetaEndSort"},
    {L"google.com", L"google.com",
     "http://www.google.com/search?q={searchTerms}&ie=utf-8&oe=utf-8&aq=t"},
    {L"en.wikipedia.org", L"wiki",
     "http://en.wikipedia.org/wiki/Special:Search?search={searchTerms}"},
    {L"search.yahoo.com", L"search.yahoo.com",
     "http://search.yahoo.com/search?p={searchTerms}&ei=UTF-8"},
    {L"flickr.com", L"flickr.com",
     "http://www.flickr.com/photos/tags/?q={searchTerms}"},
    {L"imdb.com", L"imdb.com", "http://www.imdb.com/find?q={searchTerms}"},
    {L"webster.com", L"webster.com",
     "http://www.webster.com/cgi-bin/dictionary?va={searchTerms}"},
    // Search keywords.
    {L"\x4E2D\x6587", L"\x4E2D\x6587", "http://www.google.com/"},
    {L"keyword", L"keyword", "http://example.{searchTerms}.com/"},
    // in_process_importer_bridge.cc:CreateTemplateURL() will return NULL for
    // the following bookmark. Consequently, it won't be imported as a search
    // engine.
    {L"", L"", "http://%x.example.{searchTerms}.com/"},
};

const AutofillFormDataInfo kFirefoxAutofillEntries[] = {
    {"name", "John"},
    {"address", "#123 Cherry Ave"},
    {"city", "Mountain View"},
    {"zip", "94043"},
    {"n300", "+1 (408) 871-4567"},
    {"name", "john"},
    {"name", "aguantó"},
    {"address", "télévision@example.com"},
    {"city", "&$%$$$ TESTO *&*&^&^& MOKO"},
    {"zip", "WOHOOOO$$$$$$$$****"},
    {"n300", "\xe0\xa4\x9f\xe2\x97\x8c\xe0\xa4\xbe\xe0\xa4\xaf\xe0\xa4\xb0"},
    {"n300", "\xe4\xbb\xa5\xe7\x8e\xa9\xe4\xb8\xba\xe4\xb8\xbb"}
};

class FirefoxObserver : public ProfileWriter,
                        public importer::ImporterProgressObserver {
 public:
  explicit FirefoxObserver(bool use_keyword_in_json)
      : ProfileWriter(NULL),
        bookmark_count_(0),
        history_count_(0),
        password_count_(0),
        keyword_count_(0),
        use_keyword_in_json_(use_keyword_in_json) {}

  // importer::ImporterProgressObserver:
  void ImportStarted() override {}
  void ImportItemStarted(importer::ImportItem item) override {}
  void ImportItemEnded(importer::ImportItem item) override {}
  void ImportEnded() override {
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
    EXPECT_EQ(base::size(kFirefoxBookmarks), bookmark_count_);
    EXPECT_EQ(1U, history_count_);
    EXPECT_EQ(base::size(kFirefoxPasswords), password_count_);
    // The following test case from |kFirefoxKeywords| won't be imported:
    //   "http://%x.example.{searchTerms}.com/"}.
    // Hence, value of |keyword_count_| should be lower than size of
    // |kFirefoxKeywords| by 1.
    EXPECT_EQ(base::size(kFirefoxKeywords) - 1, keyword_count_);
  }

  bool BookmarkModelIsLoaded() const override {
    // Profile is ready for writing.
    return true;
  }

  bool TemplateURLServiceIsLoaded() const override { return true; }

  void AddPasswordForm(const password_manager::PasswordForm& form) override {
    PasswordInfo p = kFirefoxPasswords[password_count_];
    EXPECT_EQ(p.origin, form.url.spec());
    EXPECT_EQ(p.realm, form.signon_realm);
    EXPECT_EQ(p.action, form.action.spec());
    EXPECT_EQ(base::ASCIIToUTF16(p.username_element), form.username_element);
    EXPECT_EQ(base::ASCIIToUTF16(p.username), form.username_value);
    EXPECT_EQ(base::ASCIIToUTF16(p.password_element), form.password_element);
    EXPECT_EQ(base::ASCIIToUTF16(p.password), form.password_value);
    EXPECT_EQ(p.blacklisted, form.blocked_by_user);
    ++password_count_;
  }

  void AddHistoryPage(const history::URLRows& page,
                      history::VisitSource visit_source) override {
    ASSERT_EQ(3U, page.size());
    EXPECT_EQ("http://www.google.com/", page[0].url().spec());
    EXPECT_EQ(base::ASCIIToUTF16("Google"), page[0].title());
    EXPECT_EQ("http://www.google.com/", page[1].url().spec());
    EXPECT_EQ(base::ASCIIToUTF16("Google"), page[1].title());
    EXPECT_EQ("http://www.cs.unc.edu/~jbs/resources/perl/perl-cgi/programs/"
              "form1-POST.html", page[2].url().spec());
    EXPECT_EQ(base::ASCIIToUTF16("example form (POST)"), page[2].title());
    EXPECT_EQ(history::SOURCE_FIREFOX_IMPORTED, visit_source);
    ++history_count_;
  }

  void AddBookmarks(const std::vector<ImportedBookmarkEntry>& bookmarks,
                    const base::string16& top_level_folder_name) override {
    ASSERT_LE(bookmark_count_ + bookmarks.size(),
              base::size(kFirefoxBookmarks));
    // Importer should import the FF favorites the same as the list, in the same
    // order.
    for (size_t i = 0; i < bookmarks.size(); ++i) {
      EXPECT_NO_FATAL_FAILURE(
          TestEqualBookmarkEntry(bookmarks[i],
                                 kFirefoxBookmarks[bookmark_count_])) << i;
      ++bookmark_count_;
    }
  }

  void AddAutofillFormDataEntries(
      const std::vector<autofill::AutofillEntry>& autofill_entries) override {
    EXPECT_EQ(base::size(kFirefoxAutofillEntries), autofill_entries.size());
    for (size_t i = 0; i < base::size(kFirefoxAutofillEntries); ++i) {
      EXPECT_EQ(kFirefoxAutofillEntries[i].name,
                base::UTF16ToUTF8(autofill_entries[i].key().name()));
      EXPECT_EQ(kFirefoxAutofillEntries[i].value,
                base::UTF16ToUTF8(autofill_entries[i].key().value()));
    }
  }

  void AddKeywords(TemplateURLService::OwnedTemplateURLVector template_urls,
                   bool unique_on_host_and_path) override {
    for (const auto& turl : template_urls) {
      // The order might not be deterministic, look in the expected list for
      // that template URL.
      bool found = false;
      const base::string16& imported_keyword = turl->keyword();
      for (const auto& keyword : kFirefoxKeywords) {
        const base::string16 expected_keyword =
            base::WideToUTF16(use_keyword_in_json_ ? keyword.keyword_in_json
                                                   : keyword.keyword_in_sqlite);
        if (imported_keyword == expected_keyword) {
          EXPECT_EQ(keyword.url, turl->url());
          found = true;
          break;
        }
      }
      EXPECT_TRUE(found);
      ++keyword_count_;
    }
  }

  void AddFavicons(
      const favicon_base::FaviconUsageDataList& favicons) override {}

 private:
  ~FirefoxObserver() override {}

  size_t bookmark_count_;
  size_t history_count_;
  size_t password_count_;
  size_t keyword_count_;

  // Newer versions of Firefox can store custom keyword names in json, which
  // override the sqlite values. To be able to test both older and newer
  // versions, tests set this variable to indicate whether to expect the
  // |keyword_in_sqlite| or |keyword_in_json| values from the reference data.
  bool use_keyword_in_json_;
};

}  // namespace

// These tests need to be browser tests in order to be able to run the OOP
// import (via ExternalProcessImporterHost) which launches a utility process on
// supported platforms.
class FirefoxProfileImporterBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    // Creates a new profile in a new subdirectory in the temp directory.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath test_path = temp_dir_.GetPath().AppendASCII("ImporterTest");
    base::DeletePathRecursively(test_path);
    base::CreateDirectory(test_path);
    profile_path_ = test_path.AppendASCII("profile");
    app_path_ = test_path.AppendASCII("app");
    base::CreateDirectory(app_path_);

    // This will launch the browser test and thus needs to happen last.
    InProcessBrowserTest::SetUp();
  }

  void FirefoxImporterBrowserTest(std::string profile_dir,
                                  importer::ImporterProgressObserver* observer,
                                  ProfileWriter* writer) {
    base::FilePath data_path;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &data_path));
    data_path = data_path.AppendASCII(profile_dir);
    ASSERT_TRUE(base::CopyDirectory(data_path, profile_path_, true));

    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &data_path));
    data_path = data_path.AppendASCII("firefox3_nss");
    ASSERT_TRUE(base::CopyDirectory(data_path, profile_path_, false));

    // Create a directory to house default search engines.
    base::FilePath default_search_engine_path =
        app_path_.AppendASCII("searchplugins");
    base::CreateDirectory(default_search_engine_path);

    // Create a directory to house custom/installed search engines.
    base::FilePath custom_search_engine_path =
        profile_path_.AppendASCII("searchplugins");
    base::CreateDirectory(custom_search_engine_path);

    // Copy over search engines.
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &data_path));
    data_path = data_path.AppendASCII("firefox_searchplugins");
    base::FilePath default_search_engine_source_path =
        data_path.AppendASCII("default");
    base::FilePath custom_search_engine_source_path =
        data_path.AppendASCII("custom");
    ASSERT_TRUE(base::CopyDirectory(
        default_search_engine_source_path, default_search_engine_path, false));
    ASSERT_TRUE(base::CopyDirectory(
        custom_search_engine_source_path, custom_search_engine_path, false));

    importer::SourceProfile source_profile;
    source_profile.importer_type = importer::TYPE_FIREFOX;
    source_profile.app_path = app_path_;
    source_profile.source_path = profile_path_;
    source_profile.locale = "en-US";

    int items = importer::HISTORY | importer::PASSWORDS | importer::FAVORITES |
                importer::SEARCH_ENGINES | importer::AUTOFILL_FORM_DATA;

    // Deletes itself.
    ExternalProcessImporterHost* host = new ExternalProcessImporterHost;
    host->set_observer(observer);
    host->StartImportSettings(
        source_profile, browser()->profile(), items, writer);
    base::RunLoop().Run();
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath profile_path_;
  base::FilePath app_path_;
};

IN_PROC_BROWSER_TEST_F(FirefoxProfileImporterBrowserTest,
                       MAYBE_IMPORTER(Firefox30Importer)) {
  scoped_refptr<FirefoxObserver> observer(new FirefoxObserver(false));
  FirefoxImporterBrowserTest(
      "firefox3_profile", observer.get(), observer.get());
}

IN_PROC_BROWSER_TEST_F(FirefoxProfileImporterBrowserTest,
                       MAYBE_IMPORTER(Firefox35Importer)) {
  scoped_refptr<FirefoxObserver> observer(new FirefoxObserver(false));
  FirefoxImporterBrowserTest(
      "firefox35_profile", observer.get(), observer.get());
}

IN_PROC_BROWSER_TEST_F(FirefoxProfileImporterBrowserTest,
                       MAYBE_IMPORTER(FirefoxImporter)) {
  scoped_refptr<FirefoxObserver> observer(new FirefoxObserver(true));
  FirefoxImporterBrowserTest("firefox_profile", observer.get(), observer.get());
}

IN_PROC_BROWSER_TEST_F(FirefoxProfileImporterBrowserTest,
                       MAYBE_IMPORTER(Firefox320Importer)) {
  scoped_refptr<FirefoxObserver> observer(new FirefoxObserver(true));
  FirefoxImporterBrowserTest("firefox320_profile", observer.get(),
                             observer.get());
}
