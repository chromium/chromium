// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/base64url.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/switches.h"

using testing::AllOf;
using testing::Eq;
using testing::Matcher;
using testing::Property;
using version_info::GetProductNameAndVersionForUserAgent;

namespace autofill {
namespace {

class WindowedNetworkObserver {
 public:
  explicit WindowedNetworkObserver(Matcher<std::string> expected_upload_data)
      : expected_upload_data_(expected_upload_data),
        message_loop_runner_(new content::MessageLoopRunner) {
    interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            &WindowedNetworkObserver::OnIntercept, base::Unretained(this)));
  }

  WindowedNetworkObserver(const WindowedNetworkObserver&) = delete;
  WindowedNetworkObserver& operator=(const WindowedNetworkObserver&) = delete;

  ~WindowedNetworkObserver() = default;

  // Waits for a network request with the |expected_upload_data_|.
  void Wait() {
    message_loop_runner_->Run();
    interceptor_.reset();
  }

 private:
  // Helper to extract the value passed to a lookup in the URL. Returns "*** not
  // found ***" if the the data cannot be decoded.
  std::string GetLookupContent(const std::string& query_path) {
    if (query_path.find("/v1/pages/") == std::string::npos)
      return "*** not found ***";
    std::string payload = query_path.substr(strlen("/v1/pages/"));
    std::string decoded_payload;
    if (base::Base64UrlDecode(payload,
                              base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                              &decoded_payload)) {
      return decoded_payload;
    }
    return "*** not found ***";
  }

  bool OnIntercept(content::URLLoaderInterceptor::RequestParams* params) {
    // NOTE: This constant matches the one defined in
    // components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.cc
    static const char kDefaultAutofillServerURL[] =
        "https://content-autofill.googleapis.com/";
    DCHECK(params);
    network::ResourceRequest resource_request = params->url_request;
    if (resource_request.url.spec().find(kDefaultAutofillServerURL) ==
        std::string::npos) {
      return false;
    }

    const std::string& data =
        (resource_request.method == "GET")
            ? GetLookupContent(resource_request.url.path())
            : network::GetUploadData(resource_request);

    if (expected_upload_data_.Matches(data))
      message_loop_runner_->Quit();

    return false;
  }

 private:
  Matcher<std::string> expected_upload_data_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  std::unique_ptr<content::URLLoaderInterceptor> interceptor_;
};

class AutofillServerTest : public InProcessBrowserTest {
 public:
  AutofillServerTest() {
    scoped_feature_list_.InitWithFeatures(
        // Enabled.
        {features::test::kAutofillServerCommunication,
         features::kAutofillUseCAAddressModel,
         features::kAutofillUseFRAddressModel,
         features::kAutofillUseITAddressModel},
        // Disabled.
        {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Wait for Personal Data Manager to be fully loaded as the events about
    // being loaded may throw off the tests and cause flakiness.
    WaitForPersonalDataManagerToBeLoaded(browser()->profile());

    // Set up the HTTPS (!) server (embedded_test_server() is an HTTP server).
    // Every hostname is handled by that server.
    host_resolver()->AddRule("a.com", "127.0.0.1");
    cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    embedded_https_test_server().SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    embedded_https_test_server().RegisterRequestHandler(base::BindRepeating(
        [](const std::map<std::string, std::string>* pages,
           const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          auto it = pages->find(request.GetURL().path());
          if (it == pages->end()) {
            return nullptr;
          }
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content_type("text/html;charset=utf-8");
          response->set_content(it->second);
          return response;
        },
        base::Unretained(&pages_)));
    ASSERT_TRUE(embedded_https_test_server().InitializeAndListen());
    embedded_https_test_server().StartAcceptingConnections();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    cert_verifier_.SetUpCommandLine(command_line);
    // Slower test bots (ChromeOS, debug, etc.) are flaky due to slower loading
    // interacting with deferred commits.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  void NavigateToUrl(std::string_view relative_url) {
    NavigateParams params(
        browser(), embedded_https_test_server().GetURL("a.com", relative_url),
        ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    ui_test_utils::NavigateToURL(&params);
  }

  // Registers the response `content_html` for a given `relative_path`.
  void SetUrlContent(std::string relative_path, std::string_view content_html) {
    ASSERT_EQ(relative_path[0], '/');
    pages_[std::move(relative_path)] = content_html;
  }

 private:
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  content::ContentMockCertVerifier cert_verifier_;
  std::map<std::string, std::string> pages_;
};

MATCHER_P(EqualsUploadProto, expected_const, "") {
  AutofillUploadRequest expected = expected_const;
  AutofillUploadRequest request;
  if (!request.ParseFromString(arg))
    return false;

  // Remove metadata because it is randomized and won't match.
  EXPECT_EQ(request.upload().has_randomized_form_metadata(),
            expected.upload().has_randomized_form_metadata());
  request.mutable_upload()->clear_randomized_form_metadata();
  expected.mutable_upload()->clear_randomized_form_metadata();
  EXPECT_EQ(request.upload().field_data_size(),
            expected.upload().field_data_size());
  if (request.upload().field_data_size() !=
      expected.upload().field_data_size()) {
    return false;

  }
  for (int i = 0; i < request.upload().field_data_size(); i++) {
    request.mutable_upload()
        ->mutable_field_data(i)
        ->clear_randomized_field_metadata();
    expected.mutable_upload()
        ->mutable_field_data(i)
        ->clear_randomized_field_metadata();
  }

  // TODO(crbug.com/40792292): The language is sometimes missing from the
  // upload, making the test flaky. Add the language back to the comparison when
  // the root cause is fixed.
  request.mutable_upload()->clear_language();
  expected.mutable_upload()->clear_language();

  return request.SerializeAsString() == expected.SerializeAsString();
}

// Verify that a site with password fields will query even in the presence
// of user defined autocomplete types.
IN_PROC_BROWSER_TEST_F(AutofillServerTest, AlwaysQueryForPasswordFields) {
  // Load the test page. Expect a query request upon loading the page.
  SetUrlContent("/test.html", R"(
      <form id=test_form>
        <input type=text id=one autocomplete=username>
        <input type=text id=two autocomplete=off>
        <input type=password id=three>
        <input type=submit>
      </form>
  )");

  AutofillPageQueryRequest query;
  query.set_client_version(std::string(GetProductNameAndVersionForUserAgent()));
  auto* query_form = query.add_forms();
  query_form->set_signature(4875414400744072230U);
  query_form->set_alternative_signature(130271417830211693U);

  query_form->add_fields()->set_signature(2594484045U);
  query_form->add_fields()->set_signature(2750915947U);
  query_form->add_fields()->set_signature(116843943U);

  std::string expected_query_string;
  ASSERT_TRUE(query.SerializeToString(&expected_query_string));

  WindowedNetworkObserver query_network_observer(expected_query_string);
  NavigateToUrl("/test.html");
  query_network_observer.Wait();
}

}  // namespace
}  // namespace autofill
