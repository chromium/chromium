// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/profile_management/profile_management_navigation_throttle.h"

#include "base/base64.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/enterprise/profile_management/saml_response_parser.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace profile_management {

using testing::_;

namespace {

constexpr char kSupportedDomain[] = "https://supported.test";
constexpr char kSwitchDomain[] = "switch.test";
constexpr char kGoogleServiceLoginUrl[] = "www.google.com/a/%s/ServiceLogin";
constexpr char kTokenUrl[] = "token.test/";
constexpr char kUnmanagedUrl[] = "unmanaged.test/";
constexpr char kTestUrl[] = "foo/1";

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

class ProfileManagementNavigationThrottleTest
    : public BrowserWithTestWindowTest {
 public:
  ProfileManagementNavigationThrottleTest() = default;

  ~ProfileManagementNavigationThrottleTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    // Create the first tab so that web_contents() exists.
    AddTab(browser(), GURL(base::StrCat({"http://", kTestUrl})));
  }

  std::unique_ptr<ProfileManagementNavigationThrottle> GetNavigationThrottle(
      content::NavigationHandle* handle) {
    auto throttle =
        std::make_unique<ProfileManagementNavigationThrottle>(handle);
    throttle->ClearAttributeMapForTesting();
    return throttle;
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* main_frame() const {
    return web_contents()->GetPrimaryMainFrame();
  }
};

TEST_F(ProfileManagementNavigationThrottleTest, FeaturesDisabled) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(/*enabled_features=*/{}, /*disabled_features=*/{
                                features::kEnableProfileTokenManagement,
                                features::kThirdPartyProfileManagement});

  content::MockNavigationHandle navigation_handle(
      GURL("https://www.example.test/"), main_frame());

  auto throttle = ProfileManagementNavigationThrottle::MaybeCreateThrottleFor(
      &navigation_handle);
  ASSERT_EQ(nullptr, throttle.get());
}

TEST_F(ProfileManagementNavigationThrottleTest, ProfileCreationDisallowed) {
  base::test::ScopedFeatureList features(
      features::kEnableProfileTokenManagement);
  g_browser_process->local_state()->SetBoolean(prefs::kBrowserAddPersonEnabled,
                                               false);
  ASSERT_FALSE(profiles::IsProfileCreationAllowed());

  content::MockNavigationHandle navigation_handle(
      GURL("https://www.example.test/"), main_frame());

  auto throttle = ProfileManagementNavigationThrottle::MaybeCreateThrottleFor(
      &navigation_handle);
  ASSERT_EQ(nullptr, throttle.get());
}

TEST_F(ProfileManagementNavigationThrottleTest, UnsupportedHost) {
  base::test::ScopedFeatureList features(
      features::kEnableProfileTokenManagement);
  content::MockNavigationHandle navigation_handle(
      GURL("https://unsupported.host/"), main_frame());
  EXPECT_CALL(navigation_handle, GetResponseBody(_)).Times(0);

  auto throttle = GetNavigationThrottle(&navigation_handle);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse().action());
}

TEST_F(ProfileManagementNavigationThrottleTest, Switch_InvalidJSON) {
  base::test::ScopedFeatureList features(
      features::kEnableProfileTokenManagement);
  // Set the command line switch to an invalid JSON string.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(
      switches::kProfileManagementAttributes,
      base::StringPrintf(kJSONAttributesTemplate,
                         base::StrCat({kSwitchDomain, "\""}).c_str()));

  content::MockNavigationHandle navigation_handle(
      GURL(base::StrCat({"https://", kSwitchDomain})), main_frame());
  EXPECT_CALL(navigation_handle, GetResponseBody(_)).Times(0);

  auto throttle = GetNavigationThrottle(&navigation_handle);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse().action());
}

TEST_F(ProfileManagementNavigationThrottleTest, Switch_InvalidAttributes) {
  base::test::ScopedFeatureList features(
      features::kEnableProfileTokenManagement);
  // Set the command line switch attributes as a string instead of JSON object.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(
      switches::kProfileManagementAttributes,
      base::StringPrintf(R"({"%s": "attribute_value"})", kSwitchDomain));

  content::MockNavigationHandle navigation_handle(
      GURL(base::StrCat({"https://", kSwitchDomain})), main_frame());
  EXPECT_CALL(navigation_handle, GetResponseBody(_)).Times(0);

  auto throttle = GetNavigationThrottle(&navigation_handle);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse().action());
}

class ProfileManagementNavigationThrottleRedirectTest
    : public ProfileManagementNavigationThrottleTest {
 public:
  ProfileManagementNavigationThrottleRedirectTest() = default;
  ~ProfileManagementNavigationThrottleRedirectTest() override = default;

  void SetUp() override {
    ProfileManagementNavigationThrottleTest::SetUp();
    features_.InitWithFeatures(
        /*enabled_features=*/{features::kEnableProfileTokenManagement,
                              features::kThirdPartyProfileManagement},
        /*disabled_features=*/{});
  }

  void SetNavigationHandleExpectations(content::MockNavigationHandle& handle,
                                       const std::string& response_body) {
    ON_CALL(handle, GetResponseBody(_))
        .WillByDefault(
            [response_body](
                content::MockNavigationHandle::ResponseBodyCallback callback) {
              std::move(callback).Run(response_body);
            });
    EXPECT_CALL(handle, GetResponseBody(_)).Times(1);
  }

  std::unique_ptr<ProfileManagementNavigationThrottle> GetNavigationThrottle(
      content::NavigationHandle* handle) {
    auto throttle =
        std::make_unique<ProfileManagementNavigationThrottle>(handle);
    throttle->ClearAttributeMapForTesting();
    throttle->SetURLsForTesting(base::StrCat({"https://", kTokenUrl}),
                                base::StrCat({"https://", kUnmanagedUrl}));
    return throttle;
  }

 protected:
  base::RunLoop loop_;

 private:
  base::test::ScopedFeatureList features_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(ProfileManagementNavigationThrottleRedirectTest, InvalidEmail) {
  content::MockNavigationHandle navigation_handle(
      GURL(std::string(kSupportedDomain)), main_frame());
  SetNavigationHandleExpectations(navigation_handle,
                                  BuildSAMLResponse(kInvalidEmail));

  auto throttle = GetNavigationThrottle(&navigation_handle);
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillProcessResponse().action());
  loop_.RunUntilIdle();

  // The throttle does not navigate away from its current URL if the provided
  // domain is invalid.
  EXPECT_EQ(kTestUrl, web_contents()->GetURL().GetContent());
}

TEST_F(ProfileManagementNavigationThrottleRedirectTest, ValidEmail) {
  content::MockNavigationHandle navigation_handle(
      GURL(std::string(kSupportedDomain)), main_frame());
  SetNavigationHandleExpectations(navigation_handle,
                                  BuildSAMLResponse(kValidEmail));

  auto throttle = GetNavigationThrottle(&navigation_handle);
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillProcessResponse().action());
  loop_.RunUntilIdle();

  // The throttle navigates to the Google sign-in URL for the domain
  // corresponding to the parsed email address.
  EXPECT_EQ(base::StringPrintf(kGoogleServiceLoginUrl, kValidDomain),
            web_contents()->GetURL().GetContent());
}

TEST_F(ProfileManagementNavigationThrottleRedirectTest, EmptyDomain) {
  content::MockNavigationHandle navigation_handle(
      GURL(std::string(kSupportedDomain)), main_frame());
  SetNavigationHandleExpectations(navigation_handle,
                                  BuildSAMLResponse(std::string()));

  auto throttle = GetNavigationThrottle(&navigation_handle);
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillProcessResponse().action());
  loop_.RunUntilIdle();

  // The throttle navigates to the token-based management test URL when a token
  // is received but a domain is not.
  EXPECT_EQ(kTokenUrl, web_contents()->GetURL().GetContent());
}

TEST_F(ProfileManagementNavigationThrottleRedirectTest, EmptyDomainAndToken) {
  content::MockNavigationHandle navigation_handle(
      GURL(std::string(kSupportedDomain)), main_frame());
  SetNavigationHandleExpectations(
      navigation_handle, BuildSAMLResponse(std::string(), std::string()));

  auto throttle = GetNavigationThrottle(&navigation_handle);
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillProcessResponse().action());
  loop_.RunUntilIdle();

  // The throttle navigates to the unmanaged test URL when neither a domain nor
  // a token are received.
  EXPECT_EQ(kUnmanagedUrl, web_contents()->GetURL().GetContent());
}

TEST_F(ProfileManagementNavigationThrottleRedirectTest, InvalidDomain) {
  content::MockNavigationHandle navigation_handle(
      GURL(std::string(kSupportedDomain)), main_frame());
  SetNavigationHandleExpectations(navigation_handle,
                                  BuildSAMLResponse(kInvalidDomain));

  auto throttle = GetNavigationThrottle(&navigation_handle);
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillProcessResponse().action());
  loop_.RunUntilIdle();

  // The throttle does not navigate away from its current URL if the provided
  // domain is invalid.
  EXPECT_EQ(kTestUrl, web_contents()->GetURL().GetContent());
}

TEST_F(ProfileManagementNavigationThrottleRedirectTest, ValidDomain) {
  content::MockNavigationHandle navigation_handle(
      GURL(std::string(kSupportedDomain)), main_frame());
  SetNavigationHandleExpectations(navigation_handle,
                                  BuildSAMLResponse(kValidDomain));

  auto throttle = GetNavigationThrottle(&navigation_handle);
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillProcessResponse().action());
  loop_.RunUntilIdle();

  // The throttle navigates to the Google sign-in URL corresponding to the
  // parsed domain when the domain is valid.
  EXPECT_EQ(base::StringPrintf(kGoogleServiceLoginUrl, kValidDomain),
            web_contents()->GetURL().GetContent());
}

TEST_F(ProfileManagementNavigationThrottleRedirectTest, Switch_ValidDomain) {
  // Set a new supported domain using the command line switch.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(
      switches::kProfileManagementAttributes,
      base::StringPrintf(kJSONAttributesTemplate, kSwitchDomain));

  content::MockNavigationHandle navigation_handle(
      GURL(base::StrCat({"https://", kSwitchDomain})), main_frame());
  SetNavigationHandleExpectations(navigation_handle,
                                  BuildSAMLResponse(kValidDomain));

  auto throttle = GetNavigationThrottle(&navigation_handle);
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillProcessResponse().action());
  loop_.RunUntilIdle();

  // The throttle navigates to the Google sign-in URL corresponding to the
  // parsed domain when the domain is valid.
  EXPECT_EQ(base::StringPrintf(kGoogleServiceLoginUrl, kValidDomain),
            web_contents()->GetURL().GetContent());
}

}  // namespace profile_management
