// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/reading_list/reading_list_api.h"

#include <memory>

#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/api/reading_list/reading_list_api_constants.h"
#include "chrome/browser/extensions/api/reading_list/reading_list_event_router.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/api/reading_list.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_channel.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// Create an extension with "readingList" permission.
scoped_refptr<const Extension> CreateReadingListExtension() {
  return ExtensionBuilder("Extension with readingList permission")
      .AddPermission("readingList")
      .Build();
}

void AddReadingListEntry(ReadingListModel* reading_list_model,
                         const GURL& url,
                         const std::string& title) {
  reading_list_model->AddOrReplaceEntry(
      url, title, reading_list::EntrySource::ADDED_VIA_CURRENT_APP,
      base::TimeDelta());
}

std::unique_ptr<KeyedService> BuildReadingListEventRouter(
    content::BrowserContext* context) {
  return std::make_unique<ReadingListEventRouter>(context);
}

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* context) {
  return std::make_unique<extensions::EventRouter>(
      context, ExtensionPrefs::Get(context));
}

}  // namespace

class ReadingListApiUnitTest : public ExtensionServiceTestBase {
 public:
  ReadingListApiUnitTest() = default;
  ReadingListApiUnitTest(const ReadingListApiUnitTest&) = delete;
  ReadingListApiUnitTest& operator=(const ReadingListApiUnitTest&) = delete;
  ~ReadingListApiUnitTest() override = default;

 protected:
  Browser* browser() { return browser_.get(); }
  TestBrowserWindow* browser_window() { return browser_window_.get(); }

 private:
  void SetUp() override;
  void TearDown() override;

  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;
  ScopedCurrentChannel channel_{version_info::Channel::UNKNOWN};
};

void ReadingListApiUnitTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();

  // Create a browser window.
  browser_window_ = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), /*user_gesture*/ true);
  params.type = Browser::TYPE_NORMAL;
  params.window = browser_window_.get();
  browser_ = std::unique_ptr<Browser>(Browser::Create(params));

  ReadingListEventRouter::GetFactoryInstance()->SetTestingFactory(
      browser_context(), base::BindRepeating(&BuildReadingListEventRouter));

  EventRouterFactory::GetInstance()->SetTestingFactory(
      browser_context(), base::BindRepeating(&BuildEventRouter));

  // We need to call ReadingListEventRouterFactory::GetForProfile() in order to
  // instantiate the keyed service, since it's not created by default in unit
  // tests.
  ReadingListEventRouter::Get(browser_context());
}

void ReadingListApiUnitTest::TearDown() {
  browser_->tab_strip_model()->CloseAllTabs();
  browser_.reset();
  browser_window_.reset();
  ExtensionServiceTestBase::TearDown();
}

// Test that it is possible to add a unique URL.
TEST_F(ReadingListApiUnitTest, AddUniqueURL) {
  scoped_refptr<const Extension> extension = CreateReadingListExtension();

  static constexpr char kArgs[] =
      R"([{
          "url": "https://www.example.com",
          "title": "example of title",
          "hasBeenRead": false
        }])";
  auto function = base::MakeRefCounted<ReadingListAddEntryFunction>();
  function->set_extension(extension);
  ReadingListModel* reading_list_model =
      ReadingListModelFactory::GetForBrowserContext(profile());

  // Add the entry.
  api_test_utils::RunFunction(function.get(), kArgs, profile(),
                              api_test_utils::FunctionMode::kNone);

  EXPECT_EQ(reading_list_model->size(), 1u);

  // Verify the features of the entry.
  GURL url = GURL("https://www.example.com");
  auto entry = reading_list_model->GetEntryByURL(url);
  EXPECT_EQ(entry->URL(), url);
  EXPECT_EQ(entry->Title(), "example of title");
  EXPECT_FALSE(entry->IsRead());
}

// Test that adding a duplicate URL generates an error.
TEST_F(ReadingListApiUnitTest, AddDuplicateURL) {
  scoped_refptr<const Extension> extension = CreateReadingListExtension();

  static constexpr char kArgs[] =
      R"([{
          "url": "https://www.example.com",
          "title": "example of title",
          "hasBeenRead": false
        }])";
  auto function = base::MakeRefCounted<ReadingListAddEntryFunction>();
  function->set_extension(extension);
  ReadingListModel* reading_list_model =
      ReadingListModelFactory::GetForBrowserContext(profile());

  // Add the entry.
  api_test_utils::RunFunction(function.get(), kArgs, profile(),
                              api_test_utils::FunctionMode::kNone);

  EXPECT_EQ(reading_list_model->size(), 1u);

  // Verify the features of the entry.
  GURL url = GURL("https://www.example.com");
  auto entry = reading_list_model->GetEntryByURL(url);
  EXPECT_EQ(entry->URL(), url);
  EXPECT_EQ(entry->Title(), "example of title");
  EXPECT_FALSE(entry->IsRead());

  // Try to add a duplicate URL and expect an error.
  function = base::MakeRefCounted<ReadingListAddEntryFunction>();
  function->set_extension(extension);
  std::string error = api_test_utils::RunFunctionAndReturnError(
      function.get(), kArgs, profile(), api_test_utils::FunctionMode::kNone);
  EXPECT_EQ(error, reading_list_api_constants::kDuplicateURLError);

  // Review that the URL added earlier still exists and there is only 1 entry in
  // the Reading List.
  EXPECT_EQ(reading_list_model->size(), 1u);
  entry = reading_list_model->GetEntryByURL(url);
  EXPECT_EQ(entry->URL(), url);
  EXPECT_EQ(entry->Title(), "example of title");
  EXPECT_FALSE(entry->IsRead());
}

// Test that it is possible to remove a URL.
TEST_F(ReadingListApiUnitTest, RemoveURL) {
  scoped_refptr<const Extension> extension = CreateReadingListExtension();

  ReadingListModel* reading_list_model =
      ReadingListModelFactory::GetForBrowserContext(profile());

  ReadingListLoadObserver(reading_list_model).Wait();

  AddReadingListEntry(reading_list_model, GURL("https://www.example.com"),
                      "example of title");

  // Verify that the entry has been added.
  EXPECT_EQ(reading_list_model->size(), 1u);

  // Remove the URL that was added before.
  auto remove_function = base::MakeRefCounted<ReadingListRemoveEntryFunction>();
  remove_function->set_extension(extension);
  static constexpr char kArgs[] =
      R"([{
          "url": "https://www.example.com"
        }])";
  api_test_utils::RunFunction(remove_function.get(), kArgs, profile(),
                              api_test_utils::FunctionMode::kNone);

  // Verify the size of the reading list model.
  EXPECT_EQ(reading_list_model->size(), 0u);
}

// Test that trying to remove a URL that is not in the Reading List, generates
// an error.
TEST_F(ReadingListApiUnitTest, RemoveNonExistentURL) {
  scoped_refptr<const Extension> extension = CreateReadingListExtension();

  static constexpr char kArgs[] =
      R"([{
          "url": "https://www.example.com"
        }])";
  auto function = base::MakeRefCounted<ReadingListRemoveEntryFunction>();
  function->set_extension(extension);

  // Remove the entry.
  std::string error = api_test_utils::RunFunctionAndReturnError(
      function.get(), kArgs, profile(), api_test_utils::FunctionMode::kNone);
  EXPECT_EQ(error, reading_list_api_constants::kURLNotFoundError);
}

// Test that it is possible to update the features of an entry.
TEST_F(ReadingListApiUnitTest, UpdateEntryFeatures) {
  scoped_refptr<const Extension> extension = CreateReadingListExtension();

  ReadingListModel* reading_list_model =
      ReadingListModelFactory::GetForBrowserContext(profile());

  ReadingListLoadObserver(reading_list_model).Wait();

  AddReadingListEntry(reading_list_model, GURL("https://www.example.com"),
                      "example of title");

  // Verify that the entry has been added.
  EXPECT_EQ(reading_list_model->size(), 1u);

  // Update the entry that was added before.
  auto update_function = base::MakeRefCounted<ReadingListUpdateEntryFunction>();
  update_function->set_extension(extension);
  static constexpr char kArgs[] =
      R"([{
          "url": "https://www.example.com",
          "title": "Title",
          "hasBeenRead": true
        }])";
  api_test_utils::RunFunction(update_function.get(), kArgs, profile(),
                              api_test_utils::FunctionMode::kNone);

  // Verify that the size of the reading list model is still the same.
  EXPECT_EQ(reading_list_model->size(), 1u);

  // Verify the features of the entry.
  GURL url = GURL("https://www.example.com");
  auto entry = reading_list_model->GetEntryByURL(url);
  EXPECT_EQ(entry->URL(), url);
  EXPECT_EQ(entry->Title(), "Title");
  EXPECT_TRUE(entry->IsRead());
}

// Test that trying to update an entry by providing only the URL, generates an
// error.
TEST_F(ReadingListApiUnitTest, UpdateEntryOnlyWithTheURL) {
  scoped_refptr<const Extension> extension = CreateReadingListExtension();

  ReadingListModel* reading_list_model =
      ReadingListModelFactory::GetForBrowserContext(profile());

  ReadingListLoadObserver(reading_list_model).Wait();

  AddReadingListEntry(reading_list_model, GURL("https://www.example.com"),
                      "example of title");

  // Verify that the entry has been added.
  EXPECT_EQ(reading_list_model->size(), 1u);

  // Update the entry that was added before.
  auto update_function = base::MakeRefCounted<ReadingListUpdateEntryFunction>();
  update_function->set_extension(extension);
  static constexpr char kArgs[] =
      R"([{
          "url": "https://www.example.com",
        }])";
  std::string error = api_test_utils::RunFunctionAndReturnError(
      update_function.get(), kArgs, profile(),
      api_test_utils::FunctionMode::kNone);
  EXPECT_EQ(error, reading_list_api_constants::kNoUpdateProvided);

  // Verify that the size of the reading list model is still the same.
  EXPECT_EQ(reading_list_model->size(), 1u);

  // Verify the features of the entry.
  GURL url = GURL("https://www.example.com");
  auto entry = reading_list_model->GetEntryByURL(url);
  EXPECT_EQ(entry->URL(), url);
  EXPECT_EQ(entry->Title(), "example of title");
  EXPECT_FALSE(entry->IsRead());
}

// Test that it is possible to retrieve all the entries.
TEST_F(ReadingListApiUnitTest, RetrieveAllEntries) {
  scoped_refptr<const Extension> extension = CreateReadingListExtension();

  ReadingListModel* reading_list_model =
      ReadingListModelFactory::GetForBrowserContext(profile());

  ReadingListLoadObserver(reading_list_model).Wait();

  AddReadingListEntry(reading_list_model, GURL("https://www.example.com"),
                      "example of title");
  AddReadingListEntry(reading_list_model, GURL("https://www.example2.com"),
                      "Title #2");

  // Verify that the entries have been added.
  EXPECT_EQ(reading_list_model->size(), 2u);

  // Retrieve all the entries in the Reading List.
  auto update_function = base::MakeRefCounted<ReadingListQueryFunction>();
  update_function->set_extension(extension);
  static constexpr char kArgs[] = "[{}]";

  auto entries = api_test_utils::RunFunctionAndReturnSingleResult(
      update_function.get(), kArgs, profile(),
      api_test_utils::FunctionMode::kNone);

  // Verify that all the entries were retrieved.
  EXPECT_EQ(entries.value().GetList().size(), 2u);

  // Verify that the size of the reading list model is still the same.
  EXPECT_EQ(reading_list_model->size(), 2u);
}

// Test that it is possible to retrieve entries with certain features.
TEST_F(ReadingListApiUnitTest, RetrieveCertainEntries) {
  scoped_refptr<const Extension> extension = CreateReadingListExtension();

  ReadingListModel* reading_list_model =
      ReadingListModelFactory::GetForBrowserContext(profile());

  ReadingListLoadObserver(reading_list_model).Wait();

  AddReadingListEntry(reading_list_model, GURL("https://www.example.com"),
                      "example of title");
  AddReadingListEntry(reading_list_model, GURL("https://www.example2.com"),
                      "Example");
  AddReadingListEntry(reading_list_model, GURL("https://www.example3.com"),
                      "Example");

  // Verify that the entries have been added.
  EXPECT_EQ(reading_list_model->size(), 3u);

  // Retrieve entries whose title is "Example".
  auto update_function = base::MakeRefCounted<ReadingListQueryFunction>();
  update_function->set_extension(extension);
  static constexpr char kArgs[] =
      R"([{
          "title": "Example"
        }])";
  auto entries = api_test_utils::RunFunctionAndReturnSingleResult(
      update_function.get(), kArgs, profile(),
      api_test_utils::FunctionMode::kNone);

  // Verify that 2 entries were retrieved and that their title is "Example".
  EXPECT_EQ(entries.value().GetList().size(), 2u);
  static constexpr char kExpectedJson[] =
      R"([{
           "url": "https://www.example2.com/",
           "title": "Example",
           "hasBeenRead": false
         },
         {
           "url": "https://www.example3.com/",
           "title": "Example",
           "hasBeenRead": false
         }])";
  EXPECT_THAT(entries.value().GetList(), base::test::IsJson(kExpectedJson));

  // Verify that the size of the reading list model is still the same.
  EXPECT_EQ(reading_list_model->size(), 3u);
}

// Test that it is possible not to retrieve entries.
TEST_F(ReadingListApiUnitTest, NoEntriesRetrieved) {
  scoped_refptr<const Extension> extension = CreateReadingListExtension();

  ReadingListModel* reading_list_model =
      ReadingListModelFactory::GetForBrowserContext(profile());

  ReadingListLoadObserver(reading_list_model).Wait();

  AddReadingListEntry(reading_list_model, GURL("https://www.example.com"),
                      "example of title");

  // Query for an entry.
  auto update_function = base::MakeRefCounted<ReadingListQueryFunction>();
  update_function->set_extension(extension);
  static constexpr char kArgs[] =
      R"([{
          "url": "https://www.example.com",
          "title": "Title",
          "hasBeenRead": false
        }])";
  auto entries = api_test_utils::RunFunctionAndReturnSingleResult(
      update_function.get(), kArgs, profile(),
      api_test_utils::FunctionMode::kNone);

  // Verify that no entries were retrieved.
  EXPECT_EQ(entries.value().GetList().size(), 0u);
}

// Test that adding an entry generates an event.
TEST_F(ReadingListApiUnitTest, ReadingListOnEntryAdded) {
  TestEventRouterObserver event_observer(EventRouter::Get(browser_context()));

  ReadingListModel* const reading_list_model =
      ReadingListModelFactory::GetForBrowserContext(profile());

  ReadingListLoadObserver(reading_list_model).Wait();

  AddReadingListEntry(reading_list_model, GURL("https://www.example.com"),
                      "example of title");

  EXPECT_EQ(reading_list_model->size(), 1u);

  EXPECT_TRUE(base::Contains(event_observer.events(),
                             api::reading_list::OnEntryAdded::kEventName));
}

// Test that removing an entry generates an event.
TEST_F(ReadingListApiUnitTest, ReadingListOnEntryWillBeRemoved) {
  ReadingListModel* const reading_list_model =
      ReadingListModelFactory::GetForBrowserContext(profile());

  ReadingListLoadObserver(reading_list_model).Wait();

  const GURL url = GURL("https://www.example.com");

  AddReadingListEntry(reading_list_model, url, "example of title");
  EXPECT_EQ(reading_list_model->size(), 1u);

  TestEventRouterObserver event_observer(EventRouter::Get(browser_context()));

  reading_list_model->RemoveEntryByURL(url);
  EXPECT_EQ(reading_list_model->size(), 0u);

  EXPECT_EQ(event_observer.events().size(), 1u);
  EXPECT_TRUE(
      base::Contains(event_observer.events(),
                     api::reading_list::OnEntryWillBeRemoved::kEventName));
}

// Test that updating an entry generates an event.
TEST_F(ReadingListApiUnitTest, ReadingListOnEntryUpdated) {
  ReadingListModel* const reading_list_model =
      ReadingListModelFactory::GetForBrowserContext(profile());

  ReadingListLoadObserver(reading_list_model).Wait();

  const GURL url = GURL("https://www.example.com");

  AddReadingListEntry(reading_list_model, url, "example of title");
  EXPECT_EQ(reading_list_model->size(), 1u);
  EXPECT_EQ(reading_list_model->GetEntryByURL(url)->Title(),
            "example of title");

  TestEventRouterObserver event_observer(EventRouter::Get(browser_context()));

  reading_list_model->SetEntryTitleIfExists(url, "New title");
  EXPECT_EQ(reading_list_model->GetEntryByURL(url)->Title(), "New title");

  EXPECT_EQ(event_observer.events().size(), 1u);
  EXPECT_TRUE(base::Contains(event_observer.events(),
                             api::reading_list::OnEntryUpdated::kEventName));
}

}  // namespace extensions
