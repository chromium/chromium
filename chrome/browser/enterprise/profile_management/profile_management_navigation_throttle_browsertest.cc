// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/profile_management/profile_management_navigation_throttle.h"

#include "base/base64.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/enterprise/profile_management/saml_response_parser.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace profile_management {

namespace {

constexpr char kSwitchDomain[] = "switch.test";
constexpr char kGoogleServiceLoginUrl[] =
    "https://www.google.com/a/%s/ServiceLogin";

constexpr char kValidDomain[] = "host.test";
constexpr char kInvalidDomain[] = "host";
constexpr char kValidEmail[] = "user@host.test";
constexpr char kInvalidEmail[] = "user@host";

constexpr char kJSONAttributesTemplate[] = R"({"%s":{"name":"placeholderName",
"domain":"placeholderDomain","token":"placeholderToken"}})";

constexpr char kHTMLTemplate[] = R"(<html><body><form>
      <input name="SAMLResponse" value="%s"/></form></body></html>)";

constexpr char kSAMLResponse[] = R"(
<samlp:Response>
  <Assertion>
    <AttributeStatement>
      <Attribute Name="placeholderDomain">
        <AttributeValue>%s</AttributeValue>
      </Attribute>
      <Attribute Name="placeholderToken">
        <AttributeValue>%s</AttributeValue>
      </Attribute>
    </AttributeStatement>
  </Assertion>
</samlp:Response>)";

std::string BuildSAMLResponse(const std::string& domain,
                              const std::string& token = "token-value") {
  std::string encoded_saml_response = base::Base64Encode(
      base::StringPrintf(kSAMLResponse, domain.c_str(), token.c_str()));
  return base::StringPrintf(kHTMLTemplate, encoded_saml_response.c_str());
}

}  // namespace

class ProfileManagementNavigationThrottleTest : public InProcessBrowserTest {
 public:
  ProfileManagementNavigationThrottleTest() = default;

  ~ProfileManagementNavigationThrottleTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &ProfileManagementNavigationThrottleTest::HandleRequest,
        base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("*", "127.0.0.1");

    // Default to a valid email.
    SetSAMLResponse(kValidEmail);
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != "/saml") {
      return nullptr;
    }
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content(
        BuildSAMLResponse(current_saml_domain_, current_saml_token_));
    response->set_content_type("text/html");
    return response;
  }

  void SetSAMLResponse(const std::string& domain,
                       const std::string& token = "token-value") {
    current_saml_domain_ = domain;
    current_saml_token_ = token;
  }

  GURL GetSAMLUrl() {
    return embedded_test_server()->GetURL("supported.test", "/saml");
  }

 private:
  std::string current_saml_domain_;
  std::string current_saml_token_;
};

class ProfileManagementNavigationThrottleDisabledTest
    : public ProfileManagementNavigationThrottleTest {
 public:
  ProfileManagementNavigationThrottleDisabledTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kEnableProfileTokenManagement,
                               features::kThirdPartyProfileManagement});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class ProfileManagementNavigationThrottleEnabledTest
    : public ProfileManagementNavigationThrottleTest {
 public:
  ProfileManagementNavigationThrottleEnabledTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEnableProfileTokenManagement,
                              features::kThirdPartyProfileManagement},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ProfileManagementNavigationThrottleDisabledTest,
                       FeaturesDisabled) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSAMLUrl()));
  EXPECT_EQ(GetSAMLUrl(), web_contents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(ProfileManagementNavigationThrottleEnabledTest,
                       ProfileCreationDisallowed) {
  g_browser_process->local_state()->SetBoolean(prefs::kBrowserAddPersonEnabled,
                                               false);
  ASSERT_FALSE(profiles::IsProfileCreationAllowed());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSAMLUrl()));
  EXPECT_EQ(GetSAMLUrl(), web_contents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(ProfileManagementNavigationThrottleEnabledTest,
                       UnsupportedHost) {
  GURL url = embedded_test_server()->GetURL("unsupported.test", "/saml");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(url, web_contents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(ProfileManagementNavigationThrottleEnabledTest,
                       Switch_InvalidJSON) {
  // Set the command line switch to an invalid JSON string.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(
      switches::kProfileManagementAttributes,
      base::StringPrintf(kJSONAttributesTemplate,
                         base::StrCat({kSwitchDomain, "\""}).c_str()));

  GURL url = embedded_test_server()->GetURL(kSwitchDomain, "/saml");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(url, web_contents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(ProfileManagementNavigationThrottleEnabledTest,
                       Switch_InvalidAttributes) {
  // Set the command line switch attributes as a string instead of JSON object.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(
      switches::kProfileManagementAttributes,
      base::StringPrintf(R"({"%s": "attribute_value"})", kSwitchDomain));

  GURL url = embedded_test_server()->GetURL(kSwitchDomain, "/saml");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(url, web_contents()->GetURL());
}

class ProfileManagementNavigationThrottleRedirectTest
    : public ProfileManagementNavigationThrottleTest {
 public:
  ProfileManagementNavigationThrottleRedirectTest() {
    features_.InitWithFeatures(
        /*enabled_features=*/{features::kEnableProfileTokenManagement,
                              features::kThirdPartyProfileManagement},
        /*disabled_features=*/{});
  }
  ~ProfileManagementNavigationThrottleRedirectTest() override = default;

  void SetUpOnMainThread() override {
    ProfileManagementNavigationThrottleTest::SetUpOnMainThread();
    in_process_data_decoder_ =
        std::make_unique<data_decoder::test::InProcessDataDecoder>();
  }

 private:
  base::test::ScopedFeatureList features_;
  std::unique_ptr<data_decoder::test::InProcessDataDecoder>
      in_process_data_decoder_;
};

IN_PROC_BROWSER_TEST_F(ProfileManagementNavigationThrottleRedirectTest,
                       InvalidEmail) {
  SetSAMLResponse(kInvalidEmail);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSAMLUrl()));

  // The throttle does not navigate away from its current URL if the provided
  // domain is invalid.
  EXPECT_EQ(GetSAMLUrl(), web_contents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(ProfileManagementNavigationThrottleRedirectTest,
                       ValidEmail) {
  GURL expected_url(base::StringPrintf(kGoogleServiceLoginUrl, kValidDomain));
  content::TestNavigationObserver observer(expected_url);
  observer.WatchExistingWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSAMLUrl()));
  observer.Wait();

  // The throttle navigates to the Google sign-in URL for the domain
  // corresponding to the parsed email address.
  EXPECT_EQ(expected_url, web_contents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(ProfileManagementNavigationThrottleRedirectTest,
                       EmptyDomain) {
  SetSAMLResponse(std::string());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSAMLUrl()));

  // The throttle does not navigate away from its current URL if no domain is
  // found in the response.
  EXPECT_EQ(GetSAMLUrl(), web_contents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(ProfileManagementNavigationThrottleRedirectTest,
                       EmptyDomainAndToken) {
  SetSAMLResponse(std::string(), std::string());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSAMLUrl()));

  // The throttle does not navigate away from its current URL if neither a
  // domain nor a token are found in the response.
  EXPECT_EQ(GetSAMLUrl(), web_contents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(ProfileManagementNavigationThrottleRedirectTest,
                       InvalidDomain) {
  SetSAMLResponse(kInvalidDomain);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSAMLUrl()));

  // The throttle does not navigate away from its current URL if the provided
  // domain is invalid.
  EXPECT_EQ(GetSAMLUrl(), web_contents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(ProfileManagementNavigationThrottleRedirectTest,
                       ValidDomain) {
  SetSAMLResponse(kValidDomain);

  GURL expected_url(base::StringPrintf(kGoogleServiceLoginUrl, kValidDomain));
  content::TestNavigationObserver observer(expected_url);
  observer.WatchExistingWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSAMLUrl()));
  observer.Wait();

  // The throttle navigates to the Google sign-in URL corresponding to the
  // parsed domain when the domain is valid.
  EXPECT_EQ(expected_url, web_contents()->GetURL());
}

class ProfileManagementNavigationThrottleSwitchTest
    : public ProfileManagementNavigationThrottleRedirectTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ProfileManagementNavigationThrottleRedirectTest::SetUpCommandLine(
        command_line);
    command_line->AppendSwitchASCII(
        switches::kProfileManagementAttributes,
        base::StringPrintf(kJSONAttributesTemplate, kSwitchDomain));
  }
};

IN_PROC_BROWSER_TEST_F(ProfileManagementNavigationThrottleSwitchTest,
                       Switch_ValidDomain) {
  SetSAMLResponse(kValidDomain);

  GURL switch_url = embedded_test_server()->GetURL(kSwitchDomain, "/saml");
  GURL expected_url(base::StringPrintf(kGoogleServiceLoginUrl, kValidDomain));
  content::TestNavigationObserver observer(expected_url);
  observer.WatchExistingWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), switch_url));
  observer.Wait();

  // The throttle navigates to the Google sign-in URL corresponding to the
  // parsed domain when the domain is valid.
  EXPECT_EQ(expected_url, web_contents()->GetURL());
}

}  // namespace profile_management
