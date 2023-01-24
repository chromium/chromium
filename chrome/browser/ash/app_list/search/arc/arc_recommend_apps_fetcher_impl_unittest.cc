// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/arc/recommend_apps_fetcher_impl.h"

#include <memory>

#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/ash/app_list/search/arc/fake_recommend_apps_fetcher_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {

class AppListRecommendAppsFetcherImplTest : public testing::Test {
 public:
  AppListRecommendAppsFetcherImplTest() = default;
  ~AppListRecommendAppsFetcherImplTest() override = default;

  void SetUp() override {
    test_url_loader_factory_.SetInterceptor(base::BindRepeating(
        &AppListRecommendAppsFetcherImplTest::InterceptRequest,
        base::Unretained(this)));

    std::unique_ptr<RecommendAppsFetcherImpl> temp =
        std::make_unique<RecommendAppsFetcherImpl>(&delegate_,
                                                   &test_url_loader_factory_);
    temp->SetAndroidIdStatusForTesting(true);
    recommend_apps_fetcher_ = std::move(temp);
  }

 protected:
  network::ResourceRequest* WaitForAppListRequest() {
    if (test_url_loader_factory_.pending_requests()->size() == 0) {
      request_waiter_ = std::make_unique<base::RunLoop>();
      request_waiter_->Run();
      request_waiter_.reset();
    }
    return &test_url_loader_factory_.GetPendingRequest(0)->request;
  }

  FakeRecommendAppsFetcherDelegate delegate_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<RecommendAppsFetcher> recommend_apps_fetcher_;

 private:
  void InterceptRequest(const network::ResourceRequest& request) {
    ASSERT_EQ(
        "https://android.clients.google.com/fdfe/chrome/"
        "getfastreinstallappslist?cfecu=false",
        request.url.spec());
    if (request_waiter_)
      request_waiter_->Quit();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> request_waiter_;
};

TEST_F(AppListRecommendAppsFetcherImplTest, EmptyResponse) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  recommend_apps_fetcher_->StartDownload();

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);
  test_url_loader_factory_.AddResponse(request->url.spec(), "");

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::LOAD_ERROR,
            delegate_.WaitForResult());
}

TEST_F(AppListRecommendAppsFetcherImplTest, EmptyAppList) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  recommend_apps_fetcher_->StartDownload();

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);
  test_url_loader_factory_.AddResponse(request->url.spec(), "[]");

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::PARSE_ERROR,
            delegate_.WaitForResult());
}

TEST_F(AppListRecommendAppsFetcherImplTest, ResponseWithLeadeingBrackets) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  recommend_apps_fetcher_->StartDownload();

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  const std::string response =
      R"()]}'[{
           "title_": {"name_": "Test app 1"},
           "id_": {"id_": "test.app1"},
           "icon_": {
             "url_": {
               "privateDoNotAccessOrElseSafeUrlWrappedValue_": "http://test.app"
              }
            }
         }])";

  test_url_loader_factory_.AddResponse(request->url.spec(), response);

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::SUCCESS,
            delegate_.WaitForResult());
  base::Value::List expected_apps;
  base::Value::Dict app;
  app.Set("name", base::Value("Test app 1"));
  app.Set("icon", base::Value("http://test.app"));
  app.Set("package_name", base::Value("test.app1"));
  expected_apps.Append(std::move(app));

  EXPECT_EQ(expected_apps, delegate_.loaded_apps());
}

TEST_F(AppListRecommendAppsFetcherImplTest, MalformedJsonResponse) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  recommend_apps_fetcher_->StartDownload();

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  test_url_loader_factory_.AddResponse(request->url.spec(), ")}]'!2%^$");

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::PARSE_ERROR,
            delegate_.WaitForResult());
}

TEST_F(AppListRecommendAppsFetcherImplTest, UnexpectedResponseType) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  recommend_apps_fetcher_->StartDownload();

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  test_url_loader_factory_.AddResponse(request->url.spec(), "\"abcd\"");

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::PARSE_ERROR,
            delegate_.WaitForResult());
}

TEST_F(AppListRecommendAppsFetcherImplTest, ResponseWithMultipleApps) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  recommend_apps_fetcher_->StartDownload();

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  const std::string response =
      R"([{
           "title_": {"name_": "Test app 1"},
           "id_": {"id_": "test.app1"},
           "icon_": {
             "url_": {
               "privateDoNotAccessOrElseSafeUrlWrappedValue_": "http://test.app"
              }
            }
         }, {
           "id_": {"id_": "test.app2"}
         }])";

  test_url_loader_factory_.AddResponse(request->url.spec(), response);

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::SUCCESS,
            delegate_.WaitForResult());
  base::Value::List expected_apps;
  base::Value::Dict app1;
  app1.Set("name", base::Value("Test app 1"));
  app1.Set("icon", base::Value("http://test.app"));
  app1.Set("package_name", base::Value("test.app1"));
  expected_apps.Append(std::move(app1));

  base::Value::Dict app2;
  app2.Set("package_name", base::Value("test.app2"));
  expected_apps.Append(std::move(app2));

  EXPECT_EQ(expected_apps, delegate_.loaded_apps());
}

TEST_F(AppListRecommendAppsFetcherImplTest, InvalidAppItemsIgnored) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  recommend_apps_fetcher_->StartDownload();

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  const std::string response =
      R"([{
           "title_": {"name_": "Test app 1"},
           "id_": {"id_": "test.app1"},
           "icon_": {
             "url_": {
               "privateDoNotAccessOrElseSafeUrlWrappedValue_": "http://test.app"
              }
            }
         }, [], 2, {"id_": {"id_": "test.app2"}}, {"a": "b"}])";

  test_url_loader_factory_.AddResponse(request->url.spec(), response);

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::SUCCESS,
            delegate_.WaitForResult());
  base::Value::List expected_apps;
  base::Value::Dict app1;
  app1.Set("name", base::Value("Test app 1"));
  app1.Set("icon", base::Value("http://test.app"));
  app1.Set("package_name", base::Value("test.app1"));
  expected_apps.Append(std::move(app1));

  base::Value::Dict app2;
  app2.Set("package_name", base::Value("test.app2"));
  expected_apps.Append(std::move(app2));

  EXPECT_EQ(expected_apps, delegate_.loaded_apps());
}

TEST_F(AppListRecommendAppsFetcherImplTest, DictionaryResponse) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  recommend_apps_fetcher_->StartDownload();

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  test_url_loader_factory_.AddResponse(request->url.spec(), "{}");

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::PARSE_ERROR,
            delegate_.WaitForResult());
}

TEST_F(AppListRecommendAppsFetcherImplTest, InvalidErrorCodeType) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  recommend_apps_fetcher_->StartDownload();

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  test_url_loader_factory_.AddResponse(request->url.spec(),
                                       R"({"Error code": ""})");

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::PARSE_ERROR,
            delegate_.WaitForResult());
}

TEST_F(AppListRecommendAppsFetcherImplTest, NotEnoughAppsError) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  recommend_apps_fetcher_->StartDownload();

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  test_url_loader_factory_.AddResponse(request->url.spec(),
                                       R"({"Error code": "5"})");

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::PARSE_ERROR,
            delegate_.WaitForResult());
}

TEST_F(AppListRecommendAppsFetcherImplTest, AppListRequestFailure) {
  ASSERT_TRUE(recommend_apps_fetcher_);

  recommend_apps_fetcher_->StartDownload();

  network::ResourceRequest* request = WaitForAppListRequest();
  ASSERT_TRUE(request);

  test_url_loader_factory_.AddResponse(request->url.spec(), "",
                                       net::HTTP_BAD_REQUEST);

  EXPECT_EQ(FakeRecommendAppsFetcherDelegate::Result::LOAD_ERROR,
            delegate_.WaitForResult());
}

}  // namespace app_list::test
