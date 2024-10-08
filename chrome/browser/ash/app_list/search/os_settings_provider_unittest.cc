// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/os_settings_provider.h"

#include <memory>

#include "ash/constants/web_app_id_constants.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_handler.h"
#include "chrome/browser/ui/webui/ash/settings/test_support/fake_hierarchy.h"
#include "chrome/browser/ui/webui/ash/settings/test_support/fake_os_settings_sections.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/services/app_service/public/cpp/stub_icon_loader.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {

namespace mojom {
using ::chromeos::settings::mojom::kBluetoothSectionPath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

using SettingsResultPtr = ::ash::settings::mojom::SearchResultPtr;
using SearchResult = ::ash::settings::mojom::SearchResult;
using SearchResultIcon = ::ash::settings::mojom::SearchResultIcon;
using SearchResultType = ::ash::settings::mojom::SearchResultType;
using SearchResultDefaultRank = ::ash::settings::mojom::SearchResultDefaultRank;
using SearchResultIdentifier = ::ash::settings::mojom::SearchResultIdentifier;
using SearchCallback = base::OnceCallback<void(std::vector<SettingsResultPtr>)>;

SettingsResultPtr NewSettingsResult(const std::string& url,
                                    const std::u16string& title,
                                    double relevance,
                                    mojom::Setting setting) {
  SettingsResultPtr result = SearchResult::New();

  result->id = SearchResultIdentifier::NewSetting(setting);
  result->type = SearchResultType::kSetting;
  result->relevance_score = relevance;
  result->url_path_with_parameters = url;
  result->settings_page_hierarchy = {u"setting", u"setting details"};
  result->text = title;
  result->canonical_text = title;
  result->was_generated_from_text_match = false;
  result->default_rank = SearchResultDefaultRank::kMedium;
  return result;
}

SettingsResultPtr NewSubpageResult(const std::string& url,
                                   const std::u16string& text,
                                   double relevance,
                                   mojom::Subpage subpage) {
  SettingsResultPtr result = SearchResult::New();

  result->id = SearchResultIdentifier::NewSubpage(subpage);
  result->type = SearchResultType::kSubpage;
  result->relevance_score = relevance;
  result->url_path_with_parameters = url;
  result->settings_page_hierarchy = {u"subpage", u"subpage details"};
  result->text = text;
  result->canonical_text = text;
  result->was_generated_from_text_match = false;
  result->default_rank = SearchResultDefaultRank::kMedium;
  return result;
}

SettingsResultPtr NewSectionResult(const std::string& url,
                                   const std::u16string& text,
                                   double relevance,
                                   mojom::Section section) {
  SettingsResultPtr result = SearchResult::New();

  result->id = SearchResultIdentifier::NewSection(section);
  result->type = SearchResultType::kSection;
  result->relevance_score = relevance;
  result->url_path_with_parameters = url;
  result->settings_page_hierarchy = {u"section", u"section details"};
  result->text = text;
  result->canonical_text = text;
  result->was_generated_from_text_match = false;
  result->default_rank = SearchResultDefaultRank::kMedium;
  return result;
}

class MockSearchHandler : public ash::settings::SearchHandler {
 public:
  MockSearchHandler(ash::settings::SearchTagRegistry* search_tag_registry,
                    ash::settings::OsSettingsSections* sections,
                    ash::settings::Hierarchy* hierarchy,
                    ash::local_search_service::LocalSearchServiceProxy*
                        local_search_service_proxy)
      : ash::settings::SearchHandler(search_tag_registry,
                                     sections,
                                     hierarchy,
                                     local_search_service_proxy),
        search_tag_registry_(search_tag_registry),
        sections_(sections),
        hierarchy_(hierarchy) {}
  ~MockSearchHandler() override = default;

  MockSearchHandler(const MockSearchHandler& other) = delete;
  MockSearchHandler& operator=(const MockSearchHandler& other) = delete;

  void Search(const std::u16string& query,
              uint32_t max_num_results,
              ash::settings::mojom::ParentResultBehavior parent_result_behavior,
              SearchCallback callback) override {
    std::move(callback).Run(std::move(results_));
  }

  // Manually add in results which will be returned by the Search function.
  void SetNextResults(std::vector<SettingsResultPtr> results) {
    results_ = std::move(results);
  }

  raw_ptr<ash::settings::SearchTagRegistry> search_tag_registry_;
  raw_ptr<ash::settings::OsSettingsSections> sections_;
  raw_ptr<ash::settings::Hierarchy> hierarchy_;
  std::vector<SettingsResultPtr> results_;
};

}  // namespace

class OsSettingsProviderTest : public testing::Test {
 public:
  OsSettingsProviderTest()
      : local_search_service_proxy_(
            std::make_unique<
                ash::local_search_service::LocalSearchServiceProxy>(
                /*for_testing=*/true)),
        search_tag_registry_(local_search_service_proxy_.get()),
        fake_hierarchy_(&fake_sections_),
        mock_handler_(&search_tag_registry_,
                      &fake_sections_,
                      &fake_hierarchy_,
                      local_search_service_proxy_.get()) {}
  OsSettingsProviderTest(const OsSettingsProviderTest&) = delete;
  OsSettingsProviderTest& operator=(const OsSettingsProviderTest&) = delete;
  ~OsSettingsProviderTest() override = default;

  void SetUp() override {
    search_controller_ = std::make_unique<TestSearchController>();
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("name");

    apps::StubIconLoader stub_icon_loader;
    apps::AppServiceProxyFactory::GetForProfile(profile_)
        ->OverrideInnerIconLoaderForTesting(&stub_icon_loader);

    // Insert dummy map values so that the stub_icon_loader knows of the app.
    stub_icon_loader.update_version_by_app_id_[web_app::kOsSettingsAppId] = 1;

    // Populate the fake hierarchy with data.
    fake_hierarchy_.AddSubpageMetadata(
        IDS_SETTINGS_BLUETOOTH_DEVICE_DETAIL_NAME, mojom::Section::kBluetooth,
        mojom::Subpage::kBluetoothDevices, SearchResultIcon::kBluetooth,
        SearchResultDefaultRank::kMedium, mojom::kBluetoothSectionPath);
    fake_hierarchy_.AddSubpageMetadata(
        IDS_SETTINGS_BLUETOOTH_SAVED_DEVICES, mojom::Section::kBluetooth,
        mojom::Subpage::kBluetoothSavedDevices, SearchResultIcon::kBluetooth,
        SearchResultDefaultRank::kMedium, mojom::kBluetoothSectionPath,
        std::make_optional(mojom::Subpage::kBluetoothDevices));
    fake_hierarchy_.AddSettingMetadata(mojom::Section::kPrinting,
                                       mojom::Setting::kAddPrinter);
    fake_hierarchy_.AddSettingMetadata(mojom::Section::kPrinting,
                                       mojom::Setting::kSavedPrinters);
    fake_hierarchy_.AddSettingMetadata(
        mojom::Section::kBluetooth, mojom::Setting::kFastPairSavedDevices,
        std::make_optional(mojom::Subpage::kBluetoothSavedDevices));

    auto provider = std::make_unique<OsSettingsProvider>(profile_);
    provider->MaybeInitialize(&mock_handler_, &fake_hierarchy_);
    provider_ = provider.get();
    search_controller_->AddProvider(std::move(provider));
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    provider_ = nullptr;
    search_controller_.reset();
    profile_ = nullptr;
    profile_manager_->DeleteTestingProfile("name");
  }

  const SearchProvider::Results& results() {
    return search_controller_->last_results();
  }
  MockSearchHandler* mock_handler() { return &mock_handler_; }

  // Starts a search and waits for the query to be sent.
  void StartSearch(const std::u16string& query) {
    search_controller_->StartSearch(query);
    base::RunLoop().RunUntilIdle();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ash::local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_;
  std::unique_ptr<TestSearchController> search_controller_;
  ash::settings::SearchTagRegistry search_tag_registry_;
  ash::settings::FakeOsSettingsSections fake_sections_;
  ash::settings::FakeHierarchy fake_hierarchy_;
  MockSearchHandler mock_handler_;
  session_manager::SessionManager session_manager_;

 private:
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;
  raw_ptr<OsSettingsProvider> provider_;
};

TEST_F(OsSettingsProviderTest, Basic) {
  // Manually add in results from the mocked search handler.
  std::vector<SettingsResultPtr> settings_results;
  settings_results.emplace_back(NewSettingsResult(
      "www.open.com", u"Add Printer", 0.5, mojom::Setting::kAddPrinter));
  settings_results.emplace_back(NewSettingsResult(
      "www.close.com", u"Saved Printer", 0.6, mojom::Setting::kSavedPrinters));
  mock_handler()->SetNextResults(std::move(settings_results));

  // Should not return results if the query is too short.
  StartSearch(u"on");
  EXPECT_TRUE(results().empty());

  StartSearch(u"query");
  ASSERT_EQ(2u, results().size());
  EXPECT_EQ(results()[0]->title(), u"Add Printer");
  EXPECT_EQ(results()[0]->relevance(), 0.5);
  EXPECT_EQ(results()[0]->result_type(), ResultType::kOsSettings);
  EXPECT_EQ(results()[0]->category(), Category::kSettings);
  EXPECT_EQ(results()[0]->display_type(), DisplayType::kList);
  EXPECT_EQ(results()[0]->metrics_type(), ash::OS_SETTINGS);
  EXPECT_EQ(results()[0]->accessible_name(),
            u"Add Printer, setting details, setting");
  EXPECT_EQ(results()[0]->details(), u"setting details");
  EXPECT_TRUE(results()[0]->icon().dimension);
}

TEST_F(OsSettingsProviderTest, WillFilterResultsBelowTheScoreThreshold) {
  std::vector<SettingsResultPtr> settings_results;
  settings_results.emplace_back(NewSettingsResult(
      "www.open.com", u"Add Printer", 0.1, mojom::Setting::kAddPrinter));
  settings_results.emplace_back(
      NewSettingsResult("www.savedFast.com", u"Fast Pair Saved Devices", 0.2,
                        mojom::Setting::kFastPairSavedDevices));
  settings_results.emplace_back(NewSettingsResult(
      "www.close.com", u"Saved Printer", 0.6, mojom::Setting::kSavedPrinters));
  mock_handler()->SetNextResults(std::move(settings_results));

  // Should only return the results with a high enough relevance.
  StartSearch(u"query");
  ASSERT_EQ(1u, results().size());
  EXPECT_EQ(results()[0]->title(), u"Saved Printer");
}

TEST_F(OsSettingsProviderTest, WillFilterOutDuplicateURLs) {
  std::vector<SettingsResultPtr> settings_results;
  settings_results.emplace_back(NewSettingsResult(
      "www.open.com", u"Add Printer", 0.8, mojom::Setting::kAddPrinter));
  settings_results.emplace_back(
      NewSettingsResult("www.open.com", u"Fast Pair Saved Devices", 0.8,
                        mojom::Setting::kFastPairSavedDevices));
  settings_results.emplace_back(NewSettingsResult(
      "www.close.com", u"Saved Printer", 0.6, mojom::Setting::kSavedPrinters));
  mock_handler()->SetNextResults(std::move(settings_results));

  // The second result will be filtered as its URL is identical to the first
  // result.
  StartSearch(u"query");
  ASSERT_EQ(2u, results().size());
  EXPECT_EQ(results()[0]->title(), u"Add Printer");
  EXPECT_EQ(results()[1]->title(), u"Saved Printer");
}

TEST_F(OsSettingsProviderTest, WillFilterOutAlternateMatches) {
  std::vector<SettingsResultPtr> settings_results;
  settings_results.emplace_back(NewSettingsResult(
      "www.open.com", u"Add Printer", 0.8, mojom::Setting::kAddPrinter));
  SettingsResultPtr result =
      NewSettingsResult("www.open.com", u"Fast Pair Saved Devices", 0.8,
                        mojom::Setting::kFastPairSavedDevices);
  result->canonical_text = u"Saved Devices";
  settings_results.emplace_back(std::move(result));
  mock_handler()->SetNextResults(std::move(settings_results));

  // The second result will be filtered as its text does not match its canonical
  // text.
  StartSearch(u"query");
  ASSERT_EQ(1u, results().size());
  EXPECT_EQ(results()[0]->title(), u"Add Printer");
}

TEST_F(OsSettingsProviderTest,
       WillFilterOutSubpageResultsWithAHigherScoringAncestor) {
  std::vector<SettingsResultPtr> settings_results;
  settings_results.emplace_back(
      NewSubpageResult("www.bluetooth.com", u"Bluetooth Devices", 0.7,
                       mojom::Subpage::kBluetoothDevices));
  settings_results.emplace_back(
      NewSubpageResult("www.savedBlue.com", u"Saved Blue Devices", 0.6,
                       mojom::Subpage::kBluetoothSavedDevices));
  settings_results.emplace_back(
      NewSubpageResult("www.savedBluetooth.com", u"Saved Bluetooth Devices",
                       0.9, mojom::Subpage::kBluetoothSavedDevices));
  mock_handler()->SetNextResults(std::move(settings_results));

  // The second result will be filtered out as there is a higher scoring
  // ancestor also present in the results.
  StartSearch(u"query");
  ASSERT_EQ(2u, results().size());
  EXPECT_EQ(results()[0]->title(), u"Bluetooth Devices");
  EXPECT_EQ(results()[1]->title(), u"Saved Bluetooth Devices");
}

TEST_F(OsSettingsProviderTest,
       WillFilterOutSettingsResultsWithAHigherScoringSubpageAncestor) {
  std::vector<SettingsResultPtr> settings_results;
  settings_results.emplace_back(
      NewSubpageResult("www.bluetooth.com", u"Bluetooth Devices", 0.7,
                       mojom::Subpage::kBluetoothDevices));
  settings_results.emplace_back(
      NewSettingsResult("www.fastPair.com", u"Fast Pair Saved Devices", 0.6,
                        mojom::Setting::kFastPairSavedDevices));
  settings_results.emplace_back(
      NewSettingsResult("www.fastPair2.com", u"Fast Pair Saved Devices2", 0.9,
                        mojom::Setting::kFastPairSavedDevices));
  mock_handler()->SetNextResults(std::move(settings_results));

  // The second result will be filtered out as there is a higher scoring
  // ancestor also present in the results.
  StartSearch(u"query");
  ASSERT_EQ(2u, results().size());
  EXPECT_EQ(results()[0]->title(), u"Bluetooth Devices");
  EXPECT_EQ(results()[1]->title(), u"Fast Pair Saved Devices2");
}

TEST_F(OsSettingsProviderTest,
       WillFilterOutSettingsResultsWithAHigherScoringSectionAncestor) {
  std::vector<SettingsResultPtr> settings_results;
  settings_results.emplace_back(NewSectionResult(
      "www.bluetooth.com", u"Bluetooth", 0.7, mojom::Section::kBluetooth));
  settings_results.emplace_back(
      NewSettingsResult("www.fastPair.com", u"Fast Pair Saved Devices", 0.6,
                        mojom::Setting::kFastPairSavedDevices));
  settings_results.emplace_back(
      NewSettingsResult("www.fastPair2.com", u"Fast Pair Saved Devices2", 0.9,
                        mojom::Setting::kFastPairSavedDevices));
  mock_handler()->SetNextResults(std::move(settings_results));

  // The second result will be filtered out as there is a higher scoring
  // ancestor also present in the results.
  StartSearch(u"query");
  ASSERT_EQ(2u, results().size());
  EXPECT_EQ(results()[0]->title(), u"Bluetooth");
  EXPECT_EQ(results()[1]->title(), u"Fast Pair Saved Devices2");
}
}  // namespace app_list::test
