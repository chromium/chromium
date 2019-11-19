// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_service_impl.h"

#include "base/bind.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/explore_sites/catalog.pb.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kTechnologyCategoryName[] = "Technology";
const char kScienceCategoryName[] = "Science";
const char kBooksCategoryName[] = "Books";
const char kCountryCode[] = "zz";
const char kSite1UrlNoTrailingSlash[] = "https://example.com";
const char kSite1Url[] = "https://example.com/";
const char kSite2Url[] = "https://sample.com/";
const char kSite4Url[] = "https://exemplar.com/";
const char kSite1Name[] = "example";
const char kSite2Name[] = "sample";
const char kSite3Name[] = "exemplar";
const char kAcceptLanguages[] = "en-US,en;q=0.5";
}  // namespace

namespace explore_sites {

using testing::HasSubstr;
using testing::Not;

class ExploreSitesServiceImplTest : public testing::Test {
 public:
  ExploreSitesServiceImplTest();
  ~ExploreSitesServiceImplTest() override = default;

  void SetUp() override {
    std::unique_ptr<ExploreSitesStore> store =
        std::make_unique<ExploreSitesStore>(
            task_environment_.GetMainThreadTaskRunner());
    auto history_stats_reporter =
        std::make_unique<HistoryStatisticsReporter>(nullptr, nullptr);
    service_ = std::make_unique<ExploreSitesServiceImpl>(
        std::move(store),
        std::make_unique<TestURLLoaderFactoryGetter>(
            test_shared_url_loader_factory_),
        std::move(history_stats_reporter));
    success_ = false;
    test_data_ = CreateTestDataProto();
    mostly_valid_test_data_ = CreateMostlyValidTestDataProto();
    bad_test_data_ = CreateBadTestDataProto();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void UpdateCatalogDoneCallback(bool success) {
    success_ = success;
    callback_count_++;
  }

  void CatalogCallback(
      GetCatalogStatus status,
      std::unique_ptr<std::vector<ExploreSitesCategory>> categories) {
    database_status_ = status;
    if (categories != nullptr) {
      database_categories_ = std::move(categories);
    }
  }
  void OverrideFinchCountry(std::string country_code) {
    const char kCountryOverride[] = "country_override";
    SetUpExperimentOption(kCountryOverride, country_code);
  }

  void EnableFeatureWithNoOptions() { SetUpExperimentOption("", ""); }

  void SetUpExperimentOption(std::string option, std::string data) {
    base::FieldTrialParams params = {{option, data}};
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        chrome::android::kExploreSites, params);
  }

  bool success() const { return success_; }
  int callback_count() const { return callback_count_; }

  GetCatalogStatus database_status() { return database_status_; }
  std::vector<ExploreSitesCategory>* database_categories() {
    return database_categories_.get();
  }

  ExploreSitesServiceImpl* service() { return service_.get(); }

  std::string test_data() { return test_data_; }

  std::string mostly_valid_test_data() { return mostly_valid_test_data_; }

  std::string bad_test_data() { return bad_test_data_; }

  void PumpLoop() { task_environment_.RunUntilIdle(); }

  std::string CreateTestDataProto();
  std::string CreateMostlyValidTestDataProto();
  std::string CreateBadTestDataProto();

  void SimulateFetcherData(const std::string& response_data);
  void SimulateFetchFailure();

  const base::HistogramTester* histograms() const {
    return histogram_tester_.get();
  }

  network::TestURLLoaderFactory::PendingRequest* GetLastPendingRequest();

  void ValidateTestCatalog();

 private:
  class TestURLLoaderFactoryGetter
      : public ExploreSitesServiceImpl::URLLoaderFactoryGetter {
   public:
    explicit TestURLLoaderFactoryGetter(
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
        : url_loader_factory_(url_loader_factory) {}
    scoped_refptr<network::SharedURLLoaderFactory> GetFactory() override {
      return url_loader_factory_;
    }

   private:
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
    DISALLOW_COPY_AND_ASSIGN(TestURLLoaderFactoryGetter);
  };

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<explore_sites::ExploreSitesServiceImpl> service_;
  bool success_;
  int callback_count_;
  GetCatalogStatus database_status_;
  std::unique_ptr<std::vector<ExploreSitesCategory>> database_categories_;
  std::string test_data_;
  std::string mostly_valid_test_data_;
  std::string bad_test_data_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  network::ResourceRequest last_resource_request_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO,
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};

  DISALLOW_COPY_AND_ASSIGN(ExploreSitesServiceImplTest);
};

ExploreSitesServiceImplTest::ExploreSitesServiceImplTest()
    : success_(false),
      callback_count_(0),
      test_shared_url_loader_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)) {}

// Called by tests - response_data is the data we want to go back as the
// response from the network.
void ExploreSitesServiceImplTest::SimulateFetcherData(
    const std::string& response_data) {
  PumpLoop();

  DCHECK(test_url_loader_factory_.pending_requests()->size() > 0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetLastPendingRequest()->request.url.spec(), response_data, net::HTTP_OK,
      network::TestURLLoaderFactory::kMostRecentMatch);
}

// Called by tests - Will return a http failure.
void ExploreSitesServiceImplTest::SimulateFetchFailure() {
  PumpLoop();

  DCHECK(test_url_loader_factory_.pending_requests()->size() > 0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetLastPendingRequest()->request.url.spec(), "", net::HTTP_BAD_REQUEST,
      network::TestURLLoaderFactory::kMostRecentMatch);
}

// Helper to check the next request for the network.
network::TestURLLoaderFactory::PendingRequest*
ExploreSitesServiceImplTest::GetLastPendingRequest() {
  EXPECT_GT(test_url_loader_factory_.pending_requests()->size(), 0U)
      << "No pending request!";
  network::TestURLLoaderFactory::PendingRequest* request =
      &(test_url_loader_factory_.pending_requests()->back());
  return request;
}

void ExploreSitesServiceImplTest::ValidateTestCatalog() {
  EXPECT_EQ(GetCatalogStatus::kSuccess, database_status());
  EXPECT_NE(nullptr, database_categories());
  EXPECT_EQ(1U, database_categories()->size());

  const ExploreSitesCategory& database_category = database_categories()->at(0);
  EXPECT_EQ(Category_CategoryType_TECHNOLOGY, database_category.category_type);
  EXPECT_EQ(std::string(kTechnologyCategoryName), database_category.label);
  EXPECT_EQ(2U, database_category.sites.size());

  // Since the site name and url might come back in a different order than we
  // started with, accept either order as long as one name and url match.
  EXPECT_NE(database_category.sites[0].site_id,
            database_category.sites[1].site_id);
  std::string site1Url = database_category.sites[0].url.spec();
  std::string site2Url = database_category.sites[1].url.spec();
  std::string site1Name = database_category.sites[0].title;
  std::string site2Name = database_category.sites[1].title;
  EXPECT_TRUE(site1Url == kSite1Url || site1Url == kSite2Url);
  EXPECT_TRUE(site2Url == kSite1Url || site2Url == kSite2Url);
  EXPECT_TRUE(site1Name == kSite1Name || site1Name == kSite2Name);
  EXPECT_TRUE(site2Name == kSite1Name || site2Name == kSite2Name);
}

// This is a helper to generate testing data to use in tests.
std::string ExploreSitesServiceImplTest::CreateTestDataProto() {
  std::string serialized_protobuf;
  explore_sites::GetCatalogResponse catalog_response;
  catalog_response.set_version_token("abcd");
  explore_sites::Catalog* catalog = catalog_response.mutable_catalog();
  explore_sites::Category* category = catalog->add_categories();
  explore_sites::Site* site1 = category->add_sites();
  explore_sites::Site* site2 = category->add_sites();

  // Fill in fields we need to add to the EoS database.

  // Create two sites.  We create one with no trailing slash.  The trailing
  // slash should be added when we convert it to a GURL for canonicalization.
  site1->set_site_url(kSite1UrlNoTrailingSlash);
  site1->set_title(kSite1Name);
  site2->set_site_url(kSite2Url);
  site2->set_title(kSite2Name);

  // Create one category, technology.
  category->set_type(Category_CategoryType_TECHNOLOGY);
  category->set_localized_title(kTechnologyCategoryName);

  // Serialize the catalog into a string.
  catalog_response.SerializeToString(&serialized_protobuf);

  // Print out the string
  DVLOG(1) << "test data proto '" << serialized_protobuf << "'";

  return serialized_protobuf;
}

// This is a helper to generate testing data to use in tests.
std::string ExploreSitesServiceImplTest::CreateMostlyValidTestDataProto() {
  std::string serialized_protobuf;
  explore_sites::GetCatalogResponse catalog_response;
  catalog_response.set_version_token("abcd");
  explore_sites::Catalog* catalog = catalog_response.mutable_catalog();
  explore_sites::Category* category = catalog->add_categories();
  explore_sites::Site* site1 = category->add_sites();
  explore_sites::Site* site2 = category->add_sites();
  explore_sites::Site* site3 = category->add_sites();
  explore_sites::Site* site4 = category->add_sites();

  // Fill in fields we need to add to the EoS database.

  // Create some sites.  The first two are valid, the third is missing a URL,
  // the fourth is missing a title.
  site1->set_site_url(kSite1UrlNoTrailingSlash);
  site1->set_title(kSite1Name);
  site2->set_site_url(kSite2Url);
  site2->set_title(kSite2Name);
  site3->set_title(kSite3Name);
  site4->set_site_url(kSite4Url);

  // Create one category, technology.
  category->set_type(Category_CategoryType_TECHNOLOGY);
  category->set_localized_title(kTechnologyCategoryName);

  // Serialize the catalog into a string.
  catalog_response.SerializeToString(&serialized_protobuf);

  return serialized_protobuf;
}

// This is a helper to generate testing data to use in tests.  We intentionally
// create catalogs and sites with problems to make the code emit histograms. We
// create categories and sites with the following intentional defects:
// A category with a type outside the known range.
// A category with no sites.
// A category with no title.
// A category with only one site, which is invalid and gets removed.
// A site with a malformed url.
// A site with a missing title, as the only site in one category.
std::string ExploreSitesServiceImplTest::CreateBadTestDataProto() {
  std::string serialized_protobuf;
  explore_sites::GetCatalogResponse catalog_response;
  catalog_response.set_version_token("abcd");
  explore_sites::Catalog* catalog = catalog_response.mutable_catalog();
  explore_sites::Category* technology = catalog->add_categories();
  explore_sites::Category* anime = catalog->add_categories();
  explore_sites::Category* food = catalog->add_categories();
  explore_sites::Category* books = catalog->add_categories();
  explore_sites::Category* science = catalog->add_categories();
  explore_sites::Site* site1 = technology->add_sites();
  explore_sites::Site* site2 = technology->add_sites();
  explore_sites::Site* site3 = books->add_sites();

  // Site 1 will be a totally valid site.
  site1->set_site_url(kSite1Url);
  site1->set_title(kSite1Name);

  // Site 2 will be missing a title.
  site2->set_site_url(kSite2Url);
  site2->set_title("");

  // Site 3 will have a malformed URL.
  site3->set_site_url("123456");
  site3->set_title(kSite3Name);

  // Fill out the technology category with valid data.
  // It will get one good website, and one with a missing title.
  // It should be left intact by validation, but with only one site.
  technology->set_type(Category_CategoryType_TECHNOLOGY);
  technology->set_localized_title(kTechnologyCategoryName);

  // Fill out the anime category with a bad type value.
  anime->set_type(static_cast<Category_CategoryType>(1000));
  anime->set_localized_title("Anime");

  // Don't add a title to the food category.
  food->set_type(Category_CategoryType_FOOD);
  food->set_localized_title("");

  // The science category is valid, but has no web sites, so it should be
  // removed by validation.
  science->set_type(Category_CategoryType_SCIENCE);
  science->set_localized_title(kScienceCategoryName);

  // Fill out the books category.  It will get a website with a bad URL, and it
  // should end up getting removed since all its sites are invalid.
  books->set_type(Category_CategoryType_BOOKS);
  books->set_localized_title(kBooksCategoryName);

  // Serialize the catalog into a string.
  catalog_response.SerializeToString(&serialized_protobuf);

  // Print out the string
  DVLOG(1) << "bad test data proto '" << serialized_protobuf << "'";

  return serialized_protobuf;
}

TEST_F(ExploreSitesServiceImplTest, UpdateCatalogFromNetwork) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(chrome::android::kExploreSites);

  service()->UpdateCatalogFromNetwork(
      true /*is_immediate_fetch*/, kAcceptLanguages,
      base::BindOnce(&ExploreSitesServiceImplTest::UpdateCatalogDoneCallback,
                     base::Unretained(this)));

  // Simulate fetching using the test loader factory and test data.
  SimulateFetcherData(test_data());

  // Wait for callback to get called.
  PumpLoop();

  EXPECT_TRUE(success());

  // Get the catalog and verify the contents.

  // First call is to get update_catalog out of the way.  If GetCatalog has
  // never been called before in this session, it won't return anything, it will
  // just start the update process.  For our test, we've already put data into
  // the catalog, but GetCatalog doesn't know that.
  service()->GetCatalog(base::BindOnce(
      &ExploreSitesServiceImplTest::CatalogCallback, base::Unretained(this)));
  PumpLoop();

  ValidateTestCatalog();

  histograms()->ExpectBucketCount(
      "ExploreSites.CatalogRequestResult",
      ExploreSitesCatalogUpdateRequestResult::kNewCatalog, 1);
}

TEST_F(ExploreSitesServiceImplTest, MultipleUpdateCatalogFromNetwork) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(chrome::android::kExploreSites);

  service()->UpdateCatalogFromNetwork(
      false /*is_immediate_fetch*/, kAcceptLanguages,
      base::BindOnce(&ExploreSitesServiceImplTest::UpdateCatalogDoneCallback,
                     base::Unretained(this)));

  service()->UpdateCatalogFromNetwork(
      true /*is_immediate_fetch*/, kAcceptLanguages,
      base::BindOnce(&ExploreSitesServiceImplTest::UpdateCatalogDoneCallback,
                     base::Unretained(this)));

  service()->UpdateCatalogFromNetwork(
      true /*is_immediate_fetch*/, kAcceptLanguages,
      base::BindOnce(&ExploreSitesServiceImplTest::UpdateCatalogDoneCallback,
                     base::Unretained(this)));

  // Simulate fetching using the test loader factory and test data.
  SimulateFetcherData(test_data());

  // Wait for callback to get called.
  PumpLoop();

  EXPECT_TRUE(success());
  EXPECT_EQ(3, callback_count());
}

TEST_F(ExploreSitesServiceImplTest, GetCachedCatalogFromNetwork) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(chrome::android::kExploreSites);

  service()->UpdateCatalogFromNetwork(
      false /*is_immediate_fetch*/, kAcceptLanguages,
      base::BindOnce(&ExploreSitesServiceImplTest::UpdateCatalogDoneCallback,
                     base::Unretained(this)));
  PumpLoop();
  EXPECT_THAT(GetLastPendingRequest()->request.url.query(),
              Not(HasSubstr("version_token=abcd")));

  SimulateFetcherData(test_data());
  PumpLoop();
  EXPECT_TRUE(success());

  service()->UpdateCatalogFromNetwork(
      false /*is_immediate_fetch*/, kAcceptLanguages,
      base::BindOnce(&ExploreSitesServiceImplTest::UpdateCatalogDoneCallback,
                     base::Unretained(this)));
  PumpLoop();
  EXPECT_THAT(GetLastPendingRequest()->request.url.query(),
              HasSubstr("version_token=abcd"));

  explore_sites::GetCatalogResponse catalog_response;
  catalog_response.set_version_token("abcd");
  std::string serialized_response;
  catalog_response.SerializeToString(&serialized_response);
  SimulateFetcherData(serialized_response);

  PumpLoop();

  // Get the catalog and verify the contents.

  // First call is to get update_catalog out of the way.  If GetCatalog has
  // never been called before in this session, it won't return anything, it will
  // just start the update process.  For our test, we've already put data into
  // the catalog, but GetCatalog doesn't know that.
  // TODO(petewil): Fix get catalog so it always returns data if it has some.
  service()->GetCatalog(base::BindOnce(
      &ExploreSitesServiceImplTest::CatalogCallback, base::Unretained(this)));
  PumpLoop();

  ValidateTestCatalog();
}

TEST_F(ExploreSitesServiceImplTest, UpdateCatalogReturnsNoProtobuf) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(chrome::android::kExploreSites);

  // Pretend that a catalog has been fetched and returns with an empty protobuf.
  std::unique_ptr<std::string> empty_serialized_protobuf;
  service()->OnCatalogFetchedForTest(ExploreSitesRequestStatus::kSuccess,
                                     std::move(empty_serialized_protobuf));

  histograms()->ExpectBucketCount(
      "ExploreSites.CatalogRequestResult",
      ExploreSitesCatalogUpdateRequestResult::kExistingCatalogIsCurrent, 1);
}

TEST_F(ExploreSitesServiceImplTest, FailedCatalogFetch) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(chrome::android::kExploreSites);

  service()->UpdateCatalogFromNetwork(
      true /*is_immediate_fetch*/, kAcceptLanguages,
      base::BindOnce(&ExploreSitesServiceImplTest::UpdateCatalogDoneCallback,
                     base::Unretained(this)));

  // Simulate fetching using the test loader factory and test data.
  SimulateFetchFailure();

  // Wait for callback to get called.
  PumpLoop();

  histograms()->ExpectBucketCount(
      "ExploreSites.CatalogRequestResult",
      ExploreSitesCatalogUpdateRequestResult::kFailure, 1);
}

TEST_F(ExploreSitesServiceImplTest, BadCatalogHistograms) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(chrome::android::kExploreSites);

  service()->UpdateCatalogFromNetwork(
      true /*is_immediate_fetch*/, kAcceptLanguages,
      base::BindOnce(&ExploreSitesServiceImplTest::UpdateCatalogDoneCallback,
                     base::Unretained(this)));

  // Simulate fetching using the test loader factory and test data.
  SimulateFetcherData(bad_test_data());

  // Wait for callback to get called.
  PumpLoop();

  // Expect that we detected flaws in the data and reported the following
  // histograms.
  histograms()->ExpectTotalCount("ExploreSites.CatalogError", 7);
  histograms()->ExpectBucketCount(
      "ExploreSites.CatalogError",
      ExploreSitesCatalogError::kCategoryMissingTitle, 1);
  histograms()->ExpectBucketCount(
      "ExploreSites.CatalogError",
      ExploreSitesCatalogError::kCategoryWithUnknownType, 1);
  histograms()->ExpectBucketCount(
      "ExploreSites.CatalogError",
      ExploreSitesCatalogError::kCategoryWithNoSites, 2);
  histograms()->ExpectBucketCount("ExploreSites.CatalogError",
                                  ExploreSitesCatalogError::kSiteWithBadUrl, 1);
  histograms()->ExpectBucketCount("ExploreSites.CatalogError",
                                  ExploreSitesCatalogError::kSiteMissingTitle,
                                  1);
}

TEST_F(ExploreSitesServiceImplTest, MostlyValidCatalogHistograms) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(chrome::android::kExploreSites);

  service()->UpdateCatalogFromNetwork(
      true /*is_immediate_fetch*/, kAcceptLanguages,
      base::BindOnce(&ExploreSitesServiceImplTest::UpdateCatalogDoneCallback,
                     base::Unretained(this)));

  // Simulate fetching using the test loader factory and test data.
  SimulateFetcherData(mostly_valid_test_data());

  // Wait for callback to get called.
  PumpLoop();

  // Expect that we detected flaws in the data and reported the following
  // histograms.
  histograms()->ExpectBucketCount("ExploreSites.CatalogError",
                                  ExploreSitesCatalogError::kSiteWithBadUrl, 1);
  histograms()->ExpectBucketCount("ExploreSites.CatalogError",
                                  ExploreSitesCatalogError::kSiteMissingTitle,
                                  1);
}

TEST_F(ExploreSitesServiceImplTest, UnparseableCatalogHistograms) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(chrome::android::kExploreSites);

  service()->UpdateCatalogFromNetwork(
      true /*is_immediate_fetch*/, kAcceptLanguages,
      base::BindOnce(&ExploreSitesServiceImplTest::UpdateCatalogDoneCallback,
                     base::Unretained(this)));

  // Simulate fetching using a URL where the code expects a serialized protobuf.
  SimulateFetcherData(kSite1Url);

  // Wait for callback to get called.
  PumpLoop();

  histograms()->ExpectBucketCount("ExploreSites.CatalogError",
                                  ExploreSitesCatalogError::kParseFailure, 1);
}

TEST_F(ExploreSitesServiceImplTest, BlacklistNonCanonicalUrls) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(chrome::android::kExploreSites);

  service()->UpdateCatalogFromNetwork(
      false /*is_immediate_fetch*/, kAcceptLanguages,
      base::BindOnce(&ExploreSitesServiceImplTest::UpdateCatalogDoneCallback,
                     base::Unretained(this)));
  // Simulate fetching using the test loader factory and test data.
  SimulateFetcherData(test_data());

  // Wait for callback to get called.
  PumpLoop();
  ASSERT_TRUE(success());
  ASSERT_EQ(1, callback_count());
  service()->GetCatalog(base::BindOnce(
      &ExploreSitesServiceImplTest::CatalogCallback, base::Unretained(this)));
  PumpLoop();
  ValidateTestCatalog();

  // This will fail if canonicalization does not work correctly because
  // kSite1Url is the canonicalized version of the URL inserted in to the
  // database.
  service()->BlacklistSite(kSite1Url);
  PumpLoop();

  service()->GetCatalog(base::BindOnce(
      &ExploreSitesServiceImplTest::CatalogCallback, base::Unretained(this)));
  PumpLoop();

  EXPECT_EQ(2U, database_categories()->at(0).sites.size());
  EXPECT_TRUE(database_categories()->at(0).sites.at(0).is_blacklisted);
  EXPECT_FALSE(database_categories()->at(0).sites.at(1).is_blacklisted);
}

TEST_F(ExploreSitesServiceImplTest, CountryCodeDefault) {
  EnableFeatureWithNoOptions();

  ASSERT_EQ("DEFAULT", service()->GetCountryCode());
  service()->UpdateCatalogFromNetwork(
      false, kAcceptLanguages,
      base::BindOnce(&ExploreSitesServiceImplTest::UpdateCatalogDoneCallback,
                     base::Unretained(this)));
  PumpLoop();
  EXPECT_THAT(GetLastPendingRequest()->request.url.query(),
              HasSubstr("country_code=DEFAULT"));
}

TEST_F(ExploreSitesServiceImplTest, CountryCodeFinch) {
  OverrideFinchCountry(kCountryCode);

  EXPECT_EQ(kCountryCode, service()->GetCountryCode());
  service()->UpdateCatalogFromNetwork(
      false, kAcceptLanguages,
      base::BindOnce(&ExploreSitesServiceImplTest::UpdateCatalogDoneCallback,
                     base::Unretained(this)));
  PumpLoop();
  EXPECT_THAT(GetLastPendingRequest()->request.url.query(),
              HasSubstr("country_code=zz"));
}

TEST_F(ExploreSitesServiceImplTest, CountryCodeOverride) {
  OverrideFinchCountry("should_not_appear_country_code");
  service()->OverrideCountryCodeForDebugging(kCountryCode);

  EXPECT_EQ(kCountryCode, service()->GetCountryCode());
  service()->UpdateCatalogFromNetwork(
      false, kAcceptLanguages,
      base::BindOnce(&ExploreSitesServiceImplTest::UpdateCatalogDoneCallback,
                     base::Unretained(this)));
  PumpLoop();
  EXPECT_THAT(GetLastPendingRequest()->request.url.query(),
              HasSubstr("country_code=zz"));
}

}  // namespace explore_sites
