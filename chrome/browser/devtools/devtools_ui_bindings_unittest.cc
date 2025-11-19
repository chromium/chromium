// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_ui_bindings.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/devtools/devtools_dispatch_http_request_params.h"
#include "chrome/browser/devtools/devtools_http_service_handler.h"
#include "chrome/browser/devtools/devtools_http_service_registry.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/url_loader_interceptor.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

class DevToolsUIBindingsTest : public testing::Test {};

TEST_F(DevToolsUIBindingsTest, SanitizeFrontendURL) {
  std::vector<std::pair<std::string, std::string>> tests = {
      {"random-string", "devtools://devtools/"},
      {"http://valid.url/but/wrong", "devtools://devtools/but/wrong"},
      {"devtools://wrong-domain/", "devtools://devtools/"},
      {"devtools://devtools/bundled/devtools.html",
       "devtools://devtools/bundled/devtools.html"},
      {"devtools://devtools:1234/bundled/devtools.html#hash",
       "devtools://devtools/bundled/devtools.html#hash"},
      {"devtools://devtools/some/random/path",
       "devtools://devtools/some/random/path"},
      {"devtools://devtools/bundled/devtools.html?debugFrontend=true",
       "devtools://devtools/bundled/devtools.html?debugFrontend=true"},
      {"devtools://devtools/bundled/devtools.html"
       "?some-flag=flag&v8only=true&debugFrontend=a"
       "&another-flag=another-flag&can_dock=false&isSharedWorker=notreally"
       "&remoteFrontend=sure",
       "devtools://devtools/bundled/devtools.html"
       "?v8only=true&debugFrontend=true"
       "&can_dock=true&isSharedWorker=true&remoteFrontend=true"},
      {"devtools://devtools/?ws=any-value-is-fine",
       "devtools://devtools/?ws=any-value-is-fine"},
      {"devtools://devtools/"
       "?service-backend=ws://localhost:9222/services",
       "devtools://devtools/"
       "?service-backend=ws://localhost:9222/services"},
      {"devtools://devtools/?remoteBase="
       "http://example.com:1234/remote-base#hash",
       "devtools://devtools/?remoteBase="
       "https://chrome-devtools-frontend.appspot.com/"
       "serve_file//#hash"},
      {"devtools://devtools/?ws=1%26evil%3dtrue",
       "devtools://devtools/?ws=1%26evil%3dtrue"},
      {"devtools://devtools/?ws=encoded-ok'",
       "devtools://devtools/?ws=encoded-ok%27"},
      {"devtools://devtools/?remoteBase="
       "https://chrome-devtools-frontend.appspot.com/some/path/"
       "@123719741873/more/path.html",
       "devtools://devtools/?remoteBase="
       "https://chrome-devtools-frontend.appspot.com/serve_file/path/"},
      {"devtools://devtools/?remoteBase="
       "https://chrome-devtools-frontend.appspot.com/serve_file/"
       "@123719741873/inspector.html%3FdebugFrontend%3Dfalse",
       "devtools://devtools/?remoteBase="
       "https://chrome-devtools-frontend.appspot.com/serve_file/"
       "@123719741873/"},
      {"devtools://devtools/bundled/inspector.html?"
       "&remoteBase=https://chrome-devtools-frontend.appspot.com/serve_file/"
       "@b4907cc5d602ff470740b2eb6344b517edecb7b9/&can_dock=true",
       "devtools://devtools/bundled/inspector.html?"
       "remoteBase=https://chrome-devtools-frontend.appspot.com/serve_file/"
       "@b4907cc5d602ff470740b2eb6344b517edecb7b9/&can_dock=true"},
      {"devtools://devtools/?remoteFrontendUrl="
       "https://chrome-devtools-frontend.appspot.com/serve_rev/"
       "@12345/inspector.html%3FdebugFrontend%3Dfalse",
       "devtools://devtools/?remoteFrontendUrl="
       "https%3A%2F%2Fchrome-devtools-frontend.appspot.com%2Fserve_rev"
       "%2F%4012345%2Finspector.html%3FdebugFrontend%3Dtrue"},
      {"devtools://devtools/?remoteFrontendUrl="
       "https://chrome-devtools-frontend.appspot.com/serve_rev/"
       "@12345/inspector.html%22></iframe>something",
       "devtools://devtools/?remoteFrontendUrl="
       "https%3A%2F%2Fchrome-devtools-frontend.appspot.com%2Fserve_rev"
       "%2F%4012345%2Finspector.html"},
      {"devtools://devtools/?remoteFrontendUrl="
       "http://domain:1234/path/rev/a/filename.html%3Fparam%3Dvalue#hash",
       "devtools://devtools/?remoteFrontendUrl="
       "https%3A%2F%2Fchrome-devtools-frontend.appspot.com%2Fserve_rev"
       "%2Frev%2Finspector.html#hash"},
      {"devtools://devtools/?remoteFrontendUrl="
       "https://chrome-devtools-frontend.appspot.com/serve_rev/"
       "@12345/devtools.html%3Fws%3Danyvalue"
       "&unencoded=value&debugFrontend=true",
       "devtools://devtools/?remoteFrontendUrl="
       "https%3A%2F%2Fchrome-devtools-frontend.appspot.com%2Fserve_rev"
       "%2F%4012345%2Fdevtools.html%3Fws%3Danyvalue"
       "&debugFrontend=true"},
      {"devtools://devtools/?remoteFrontendUrl="
       "https://chrome-devtools-frontend.appspot.com/serve_rev/"
       "@12345/inspector.html%23%27",
       "devtools://devtools/?remoteFrontendUrl="
       "https%3A%2F%2Fchrome-devtools-frontend.appspot.com%2Fserve_rev"
       "%2F%4012345%2Finspector.html"},
      {"devtools://devtools/"
       "?enabledExperiments=explosionsWhileTyping;newA11yTool",
       "devtools://devtools/"
       "?enabledExperiments=explosionsWhileTyping;newA11yTool"},
      {"devtools://devtools/?enabledExperiments=invalidExperiment$",
       "devtools://devtools/"},
      {"devtools://devtools/?panel=elements",
       "devtools://devtools/?panel=elements"},
      {"devtools://devtools/?panel=network",
       "devtools://devtools/?panel=network"},
      {"devtools://devtools/?panel=console",
       "devtools://devtools/?panel=console"},
      {"devtools://devtools/?panel=sources",
       "devtools://devtools/?panel=sources"},
      {"devtools://devtools/?panel=resources",
       "devtools://devtools/?panel=resources"},
      {"devtools://devtools/?panel=performance",
       "devtools://devtools/?panel=performance"},
      {"devtools://devtools/?panel=unsupported", "devtools://devtools/"},
  };

  for (const auto& pair : tests) {
    GURL url = GURL(pair.first);
    url = DevToolsUIBindings::SanitizeFrontendURL(url);
    EXPECT_EQ(pair.second, url.spec());
  }
}

class DevToolsUIBindingsSyncInfoTest : public testing::Test {
 public:
  void SetUp() override {
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating([](content::BrowserContext*) {
          return static_cast<std::unique_ptr<KeyedService>>(
              std::make_unique<syncer::TestSyncService>());
        }));
    sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(&profile_));
  }

 protected:
  content::BrowserTaskEnvironment browser_task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;

  TestingProfile profile_;
  raw_ptr<syncer::TestSyncService> sync_service_;
};

TEST_F(DevToolsUIBindingsSyncInfoTest, SyncDisabled) {
  sync_service_->SetSignedOut();

  base::Value::Dict info =
      DevToolsUIBindings::GetSyncInformationForProfile(&profile_);

  EXPECT_EQ(
      base::FeatureList::IsEnabled(switches::kEnablePreferencesAccountStorage),
      info.FindBool("isSyncActive").value());
}

TEST_F(DevToolsUIBindingsSyncInfoTest, PreferencesNotSynced) {
  sync_service_->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kBookmarks});

  base::Value::Dict info =
      DevToolsUIBindings::GetSyncInformationForProfile(&profile_);

  EXPECT_THAT(info.FindBool("isSyncActive"), testing::Optional(true));
  EXPECT_THAT(info.FindBool("arePreferencesSynced"), testing::Optional(false));
}

TEST_F(DevToolsUIBindingsSyncInfoTest, ImageAlwaysProvided) {
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "sync@devtools.dev", signin::ConsentLevel::kSignin);
  sync_service_->SetSignedIn(signin::ConsentLevel::kSignin, account_info);

  EXPECT_TRUE(account_info.account_image.IsEmpty());

  base::Value::Dict info =
      DevToolsUIBindings::GetSyncInformationForProfile(&profile_);

  EXPECT_EQ(*info.FindString("accountEmail"), "sync@devtools.dev");
  EXPECT_NE(info.FindString("accountImage"), nullptr);
}

// This class uses the actual implementation of the CanMakeRequest
class TestServiceHandler : public DevToolsHttpServiceHandler {
 public:
  TestServiceHandler() = default;
  ~TestServiceHandler() override = default;

  GURL BaseURL() const override { return GURL("http://localhost:8000"); }
  signin::ScopeSet OAuthScopes() const override { return {}; }
  net::NetworkTrafficAnnotationTag NetworkTrafficAnnotationTag()
      const override {
    return TRAFFIC_ANNOTATION_FOR_TESTS;
  }
};

class DevToolsHttpServiceHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    TestingProfile::Builder builder;
    profile_ = builder.Build();
    mock_handler_ = base::WrapUnique(new TestServiceHandler());

    params_.service = "unknownService";
    params_.path = "/path";
    params_.method = "GET";
  }

  DevToolsDispatchHttpRequestParams params_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestServiceHandler> mock_handler_;
};

TEST_F(DevToolsHttpServiceHandlerTest, RequestWithNullProfileFails) {
  base::test::TestFuture<std::unique_ptr<DevToolsHttpServiceHandler::Result>>
      result_future;

  mock_handler_->Request(nullptr, params_, result_future.GetCallback());

  std::unique_ptr<DevToolsHttpServiceHandler::Result> result =
      result_future.Take();

  ASSERT_TRUE(result);
  EXPECT_EQ(result->error,
            DevToolsHttpServiceHandler::Result::Error::kValidationFailed);
}

TEST_F(DevToolsHttpServiceHandlerTest, RequestWithOTRProfileFails) {
  base::test::TestFuture<std::unique_ptr<DevToolsHttpServiceHandler::Result>>
      result_future;

  auto* incognito_profile = profile_->GetPrimaryOTRProfile(true);
  mock_handler_->Request(incognito_profile, params_,
                         result_future.GetCallback());

  std::unique_ptr<DevToolsHttpServiceHandler::Result> result =
      result_future.Take();

  ASSERT_TRUE(result);
  EXPECT_EQ(result->error,
            DevToolsHttpServiceHandler::Result::Error::kValidationFailed);
}

class MockServiceHandler : public DevToolsHttpServiceHandler {
 public:
  MockServiceHandler() = default;
  ~MockServiceHandler() override = default;

  GURL BaseURL() const override { return GURL("http://localhost:8000"); }
  signin::ScopeSet OAuthScopes() const override { return {}; }
  net::NetworkTrafficAnnotationTag NetworkTrafficAnnotationTag()
      const override {
    return TRAFFIC_ANNOTATION_FOR_TESTS;
  }

  MOCK_METHOD(void,
              CanMakeRequest,
              (Profile * profile, base::OnceCallback<void(bool)> callback),
              (override));
};

class DevToolsUIBindingsDispatchHttpRequestTest : public testing::Test {
 public:
  DevToolsUIBindingsDispatchHttpRequestTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());

    // Initialize the interceptor with a callback to our custom handler.
    interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            &DevToolsUIBindingsDispatchHttpRequestTest::InterceptRequest,
            base::Unretained(this)));

    web_contents_ = web_contents_factory_.CreateWebContents(profile_.get());
    bindings_ = std::make_unique<DevToolsUIBindings>(web_contents_);

    auto registry = std::make_unique<DevToolsHttpServiceRegistry>();
    auto mock_handler = base::WrapUnique(new MockServiceHandler());
    mock_handler_ptr_ = mock_handler.get();
    registry->AddForTesting(DevToolsHttpServiceRegistry::Service(
        "mockService", {{"/getFoo", "GET"}, {"/postBar", "POST"}},
        std::move(mock_handler)));
    bindings_->SetHttpServiceRegistryForTesting(std::move(registry));
  }

 protected:
  // A struct to hold response data for our interception map.
  struct TestResponse {
    std::string headers;
    std::string body;
    net::Error net_error;
  };

  void DispatchHttpRequest(DevToolsUIBindings::DispatchCallback callback,
                           const DevToolsDispatchHttpRequestParams& params) {
    bindings_->DispatchHttpRequest(std::move(callback), params);
  }

  void ExpectCanMakeRequest(bool can_make_request) {
    EXPECT_CALL(*mock_handler_ptr_, CanMakeRequest(_, _))
        .WillOnce([=](Profile*, base::OnceCallback<void(bool)> callback) {
          std::move(callback).Run(can_make_request);
        });
  }

  // Helper to configure a response for a specific URL.
  void SetResponse(const GURL& url,
                   const std::string& headers,
                   const std::string& body,
                   net::Error net_error = net::OK) {
    response_map_[url] = {headers, body, net_error};
  }

  IdentityTestEnvironmentProfileAdaptor* identity_test_env_adaptor() {
    return identity_test_env_adaptor_.get();
  }

  content::URLLoaderInterceptor* interceptor() { return interceptor_.get(); }

  const std::optional<network::ResourceRequest>& last_request() const {
    return last_request_;
  }

 private:
  bool InterceptRequest(content::URLLoaderInterceptor::RequestParams* params) {
    last_request_ = params->url_request;
    const GURL& url = params->url_request.url;
    auto it = response_map_.find(url);

    // If a specific response is not configured, use the default response.
    if (it == response_map_.end()) {
      content::URLLoaderInterceptor::WriteResponse("HTTP/1.1 200 OK\n", "body",
                                                   params->client.get());
      return true;
    }

    const TestResponse& response = it->second;
    if (response.net_error != net::OK) {
      // Handle network errors.
      params->client->OnComplete(
          network::URLLoaderCompletionStatus(response.net_error));
      return true;
    }

    // Handle HTTP success or error responses.
    content::URLLoaderInterceptor::WriteResponse(
        response.headers, response.body, params->client.get());
    return true;
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<content::URLLoaderInterceptor> interceptor_;
  std::optional<network::ResourceRequest> last_request_;

  std::unique_ptr<TestingProfile> profile_;
  content::TestWebContentsFactory web_contents_factory_;

  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<DevToolsUIBindings> bindings_;
  raw_ptr<MockServiceHandler> mock_handler_ptr_;

  std::map<GURL, TestResponse> response_map_;
};

TEST_F(DevToolsUIBindingsDispatchHttpRequestTest,
       DispatchHttpRequestUnknownService) {
  base::Value::Dict result;
  DevToolsDispatchHttpRequestParams params;
  params.service = "unknownService";
  params.path = "/path";
  params.method = "GET";
  DispatchHttpRequest(base::BindLambdaForTesting([&](const base::Value* value) {
                        result = value->GetDict().Clone();
                      }),
                      params);

  EXPECT_EQ(*result.FindString("error"), "Service not found");
}

TEST_F(DevToolsUIBindingsDispatchHttpRequestTest,
       DispatchHttpRequestDisallowedPath) {
  base::Value::Dict result;
  base::RunLoop run_loop;
  DevToolsDispatchHttpRequestParams params;
  params.service = "mockService";
  params.path = "/disallowedPath";
  params.method = "GET";
  DispatchHttpRequest(base::BindLambdaForTesting([&](const base::Value* value) {
                        result = value->GetDict().Clone();
                        run_loop.Quit();
                      }),
                      params);
  run_loop.Run();
  EXPECT_EQ(*result.FindString("error"), "Disallowed path or method");
}

TEST_F(DevToolsUIBindingsDispatchHttpRequestTest,
       DispatchHttpRequestValidationFailure) {
  ExpectCanMakeRequest(false);

  base::RunLoop run_loop;
  base::Value::Dict result;
  DevToolsDispatchHttpRequestParams params;
  params.service = "mockService";
  params.path = "/getFoo";
  params.method = "GET";
  DispatchHttpRequest(base::BindLambdaForTesting([&](const base::Value* value) {
                        result = value->GetDict().Clone();
                        run_loop.Quit();
                      }),
                      params);
  run_loop.Run();

  EXPECT_EQ(*result.FindString("error"), "Request validation failed");
}

TEST_F(DevToolsUIBindingsDispatchHttpRequestTest,
       DispatchHttpRequestTokenFetchFailure) {
  ExpectCanMakeRequest(true);
  identity_test_env_adaptor()->identity_test_env()->MakePrimaryAccountAvailable(
      "test@google.com", signin::ConsentLevel::kSignin);
  base::RunLoop run_loop;
  base::Value::Dict result;
  DevToolsDispatchHttpRequestParams params;
  params.service = "mockService";
  params.path = "/getFoo";
  params.method = "GET";
  DispatchHttpRequest(base::BindLambdaForTesting([&](const base::Value* value) {
                        result = value->GetDict().Clone();
                        run_loop.Quit();
                      }),
                      params);

  identity_test_env_adaptor()
      ->identity_test_env()
      ->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
          GoogleServiceAuthError::FromServiceUnavailable("test_error"));
  run_loop.Run();

  EXPECT_EQ(*result.FindString("error"), "Token fetch error");
  EXPECT_EQ(*result.FindString("detail"),
            "Service unavailable; try again later (test_error).");
}

TEST_F(DevToolsUIBindingsDispatchHttpRequestTest,
       DispatchHttpRequestSuccessful) {
  ExpectCanMakeRequest(true);
  identity_test_env_adaptor()->identity_test_env()->MakePrimaryAccountAvailable(
      "test@google.com", signin::ConsentLevel::kSignin);

  base::RunLoop run_loop;
  base::Value::Dict result;
  DevToolsDispatchHttpRequestParams params;
  params.service = "mockService";
  params.path = "/getFoo";
  params.method = "GET";
  DispatchHttpRequest(base::BindLambdaForTesting([&](const base::Value* value) {
                        result = value->GetDict().Clone();
                        run_loop.Quit();
                      }),
                      params);

  identity_test_env_adaptor()
      ->identity_test_env()
      ->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          "test_token", base::Time::Max());
  run_loop.Run();

  net::HttpRequestHeaders headers = last_request()->headers;
  EXPECT_EQ(last_request()->url, GURL("http://localhost:8000/getFoo"));
  EXPECT_EQ(headers.GetHeader("Authorization"), "Bearer test_token");
  EXPECT_EQ(*result.FindString("response"), "body");
  EXPECT_EQ(*result.FindInt("statusCode"), net::HTTP_OK);
}

TEST_F(DevToolsUIBindingsDispatchHttpRequestTest,
       DispatchHttpRequestNetworkError) {
  SetResponse(GURL("http://localhost:8000/postBar"), "", "", net::ERR_FAILED);
  ExpectCanMakeRequest(true);
  identity_test_env_adaptor()->identity_test_env()->MakePrimaryAccountAvailable(
      "test@google.com", signin::ConsentLevel::kSignin);

  base::RunLoop run_loop;
  base::Value::Dict result;
  DevToolsDispatchHttpRequestParams params;
  params.service = "mockService";
  params.path = "/postBar";
  params.method = "POST";
  params.body = "{\"foo\": \"bar\"}";
  DispatchHttpRequest(base::BindLambdaForTesting([&](const base::Value* value) {
                        result = value->GetDict().Clone();
                        run_loop.Quit();
                      }),
                      params);

  identity_test_env_adaptor()
      ->identity_test_env()
      ->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          "test_token", base::Time::Max());
  run_loop.Run();

  EXPECT_EQ(*result.FindString("error"), "Request failed");
  EXPECT_EQ(*result.FindInt("netError"), net::ERR_FAILED);
  EXPECT_EQ(*result.FindInt("statusCode"), -1);
}

TEST_F(DevToolsUIBindingsDispatchHttpRequestTest,
       DispatchHttpRequestHttpError) {
  SetResponse(GURL("http://localhost:8000/postBar"),
              "HTTP/1.1 400 Internal Server Error\n\n", "Bad request detail");

  ExpectCanMakeRequest(true);
  identity_test_env_adaptor()->identity_test_env()->MakePrimaryAccountAvailable(
      "test@google.com", signin::ConsentLevel::kSignin);

  base::RunLoop run_loop;
  base::Value::Dict result;
  DevToolsDispatchHttpRequestParams params;
  params.service = "mockService";
  params.path = "/postBar";
  params.method = "POST";
  params.body = "{\"foo\": \"bar\"}";
  DispatchHttpRequest(base::BindLambdaForTesting([&](const base::Value* value) {
                        result = value->GetDict().Clone();
                        run_loop.Quit();
                      }),
                      params);

  identity_test_env_adaptor()
      ->identity_test_env()
      ->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          "test_token", base::Time::Max());
  run_loop.Run();

  EXPECT_EQ(*result.FindString("error"), "Request failed");
  EXPECT_EQ(*result.FindString("detail"), "Bad request detail");
  EXPECT_EQ(*result.FindInt("statusCode"), net::HTTP_BAD_REQUEST);
  EXPECT_EQ(*result.FindInt("netError"), net::OK);
}

TEST_F(DevToolsUIBindingsDispatchHttpRequestTest, DispatchHttpRequestWithBody) {
  ExpectCanMakeRequest(true);
  identity_test_env_adaptor()->identity_test_env()->MakePrimaryAccountAvailable(
      "test@google.com", signin::ConsentLevel::kSignin);

  base::RunLoop run_loop;
  base::Value::Dict result;
  DevToolsDispatchHttpRequestParams params;
  params.service = "mockService";
  params.path = "/postBar";
  params.method = "POST";
  params.body = "{\"foo\": \"bar\"}";
  DispatchHttpRequest(base::BindLambdaForTesting([&](const base::Value* value) {
                        result = value->GetDict().Clone();
                        run_loop.Quit();
                      }),
                      params);

  identity_test_env_adaptor()
      ->identity_test_env()
      ->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          "test_token", base::Time::Max());
  run_loop.Run();

  ASSERT_TRUE(last_request().has_value());
  ASSERT_TRUE(last_request()->request_body);
  ASSERT_EQ(last_request()->request_body->elements()->size(), 1u);
  const network::DataElement& element =
      last_request()->request_body->elements()->at(0);
  ASSERT_EQ(element.type(), network::DataElement::Tag::kBytes);
  EXPECT_EQ(element.As<network::DataElementBytes>().AsStringPiece(),
            "{\"foo\": \"bar\"}");
}

TEST_F(DevToolsUIBindingsDispatchHttpRequestTest,
       DispatchHttpRequestWithoutBody) {
  ExpectCanMakeRequest(true);
  identity_test_env_adaptor()->identity_test_env()->MakePrimaryAccountAvailable(
      "test@google.com", signin::ConsentLevel::kSignin);

  base::RunLoop run_loop;
  base::Value::Dict result;
  DevToolsDispatchHttpRequestParams params;
  params.service = "mockService";
  params.path = "/getFoo";
  params.method = "GET";
  DispatchHttpRequest(base::BindLambdaForTesting([&](const base::Value* value) {
                        result = value->GetDict().Clone();
                        run_loop.Quit();
                      }),
                      params);

  identity_test_env_adaptor()
      ->identity_test_env()
      ->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          "test_token", base::Time::Max());
  run_loop.Run();

  ASSERT_TRUE(last_request().has_value());
  EXPECT_FALSE(last_request()->request_body);
}

TEST_F(DevToolsUIBindingsDispatchHttpRequestTest,
       DispatchHttpRequestWithQueryParamsSuccessful) {
  ExpectCanMakeRequest(true);
  identity_test_env_adaptor()->identity_test_env()->MakePrimaryAccountAvailable(
      "test@google.com", signin::ConsentLevel::kSignin);

  base::RunLoop run_loop;
  base::Value::Dict result;
  DevToolsDispatchHttpRequestParams params;
  params.service = "mockService";
  params.path = "/getFoo";
  params.method = "GET";
  params.query_params["q"].push_back("test/toescape");
  params.query_params["q"].push_back("test2");

  DispatchHttpRequest(base::BindLambdaForTesting([&](const base::Value* value) {
                        result = value->GetDict().Clone();
                        run_loop.Quit();
                      }),
                      params);

  identity_test_env_adaptor()
      ->identity_test_env()
      ->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          "test_token", base::Time::Max());
  run_loop.Run();

  EXPECT_EQ(last_request()->url,
            GURL("http://localhost:8000/getFoo?q=test%2Ftoescape&q=test2"));
  EXPECT_EQ(*result.FindString("response"), "body");
  EXPECT_EQ(*result.FindInt("statusCode"), net::HTTP_OK);
}
