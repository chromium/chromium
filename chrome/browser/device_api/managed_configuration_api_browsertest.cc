// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_api/managed_configuration_api.h"

#include <optional>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/test/gtest_tags.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/device_api/managed_configuration_api_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#endif

using testing::Eq;

namespace {

constexpr char kOrigin[] = "https://example.com";
constexpr char kConfigurationUrl1[] = "/conf1.json";
constexpr char kConfigurationUrl2[] = "/conf2.json";
constexpr char kConfigurationHash1[] = "asdas9jasidjd";
constexpr char kConfigurationHash2[] = "ghi289sdfsdfk";
constexpr char kConfigurationData1[] = R"(
{
  "key1": "value1",
  "key2" : 2
}
)";
constexpr char kConfigurationData2[] = R"(
{
  "key1": "value_1",
  "key2" : "value_2"
}
)";
constexpr char kConfigurationData3[] = R"(
[1]
)";
constexpr char kKey1[] = "key1";
constexpr char kKey2[] = "key2";
constexpr char kKey3[] = "key3";
constexpr char kValue1[] = "\"value1\"";
constexpr char kValue2[] = "2";
constexpr char kValue12[] = "\"value_1\"";
constexpr char kValue22[] = "\"value_2\"";

struct ResponseTemplate {
  std::string response_body;
  bool should_post_task = false;
};

std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    std::map<std::string, ResponseTemplate> templates,
    const net::test_server::HttpRequest& request) {
  if (!base::Contains(templates, request.relative_url)) {
    return std::make_unique<net::test_server::HungResponse>();
  }

  auto response_template = templates[request.relative_url];
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response;
  if (response_template.should_post_task) {
    http_response = std::make_unique<net::test_server::DelayedHttpResponse>(
        base::Seconds(0));
  } else {
    http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  }

  http_response->set_code(net::HTTP_OK);
  http_response->set_content(response_template.response_body);
  http_response->set_content_type("text/plain");
  return http_response;
}

bool DictValueEquals(std::optional<base::Value::Dict> value,
                     const std::map<std::string, std::string>& expected) {
  DCHECK(value);
  std::map<std::string, std::string> actual;
  for (auto entry : *value) {
    if (!entry.second.is_string()) {
      return false;
    }
    actual.insert({entry.first, entry.second.GetString()});
  }

  return actual == expected;
}

class ManagedConfigurationAPITestBase : public MixinBasedInProcessBrowserTest {
 protected:
  void EnableTestServer(
      const std::map<std::string, ResponseTemplate>& templates) {
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleRequest, templates));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetConfiguration(const std::string& conf_url,
                        const std::string& conf_hash) {
    base::Value::List trusted_apps;
    base::Value::Dict entry;
    entry.Set(ManagedConfigurationAPI::kOriginKey, kOrigin);
    entry.Set(ManagedConfigurationAPI::kManagedConfigurationUrlKey,
              embedded_test_server()->GetURL(conf_url).spec());
    entry.Set(ManagedConfigurationAPI::kManagedConfigurationHashKey, conf_hash);
    trusted_apps.Append(std::move(entry));
    profile()->GetPrefs()->SetList(prefs::kManagedConfigurationPerOrigin,
                                   std::move(trusted_apps));
  }

  void ClearConfiguration() {
    profile()->GetPrefs()->SetList(prefs::kManagedConfigurationPerOrigin,
                                   base::Value::List());
  }

  std::optional<base::Value::Dict> GetValues(
      const std::vector<std::string>& keys) {
    base::test::TestFuture<std::optional<base::Value::Dict>> value_future;
    api()->GetOriginPolicyConfiguration(origin_, keys,
                                        value_future.GetCallback());
    return value_future.Take();
  }

  Profile* profile() { return browser()->profile(); }
  const url::Origin& origin() const { return origin_; }
  ManagedConfigurationAPI* api() {
    return ManagedConfigurationAPIFactory::GetForProfile(profile());
  }

 private:
  const url::Origin origin_ = url::Origin::Create(GURL(kOrigin));
};

}  // namespace

class ManagedConfigurationAPITest : public ManagedConfigurationAPITestBase,
                                    public ManagedConfigurationAPI::Observer {
 public:
  ManagedConfigurationAPITest() = default;
  ManagedConfigurationAPITest(const ManagedConfigurationAPITest&) = delete;
  ManagedConfigurationAPITest& operator=(const ManagedConfigurationAPITest&) =
      delete;
  ~ManagedConfigurationAPITest() override = default;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    api()->AddObserver(this);
  }

  void TearDownOnMainThread() override {
    api()->RemoveObserver(this);
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  void WaitForUpdate() {
    if (!updated_) {
      loop_update_ = std::make_unique<base::RunLoop>();
      loop_update_->Run();
    }
  }

  void OnManagedConfigurationChanged() override {
    if (loop_update_ && loop_update_->running()) {
      loop_update_->Quit();
      updated_ = false;
    } else {
      updated_ = true;
    }
  }

  const url::Origin& GetOrigin() const override { return origin(); }

 private:
  bool updated_ = false;
  std::unique_ptr<base::RunLoop> loop_update_;
};

IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       PRE_DataIsDownloadedAndPersists) {
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  WaitForUpdate();
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue1}, {kKey2, kValue2}}));
}

IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       DataIsDownloadedAndPersists) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-2447f309-0b17-4b53-8879-50ca6eeebc3f");

  // Intentionally do not handle requests so that data has to be read from
  // disk.
  EnableTestServer({});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue1}, {kKey2, kValue2}}));
}

IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest, AppRemovedFromPolicyList) {
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  WaitForUpdate();
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue1}, {kKey2, kValue2}}));

  ClearConfiguration();
  WaitForUpdate();
  ASSERT_EQ(GetValues({kKey1, kKey2}), std::nullopt);
}

IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest, UnknownKeys) {
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  WaitForUpdate();

  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2, kKey3}),
                              {{kKey1, kValue1}, {kKey2, kValue2}}));
}

IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest, DataUpdates) {
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData1}},
                    {kConfigurationUrl2, {kConfigurationData2}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  WaitForUpdate();
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue1}, {kKey2, kValue2}}));

  SetConfiguration(kConfigurationUrl2, kConfigurationHash2);
  WaitForUpdate();
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue12}, {kKey2, kValue22}}));
}

IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       PolicyUpdateWhileDownloadingDifferentHash) {
  EnableTestServer(
      {{kConfigurationUrl1, {kConfigurationData1, true /* post_task */}},
       {kConfigurationUrl2, {kConfigurationData2}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  SetConfiguration(kConfigurationUrl2, kConfigurationHash2);

  // Even though both requests were sent, the first one must be canceled,
  // since the configuration hash has changed.
  WaitForUpdate();
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue12}, {kKey2, kValue22}}));
}

IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       PolicyUpdateWhileDownloadingSameHash) {
  EnableTestServer(
      {{kConfigurationUrl1, {kConfigurationData1, true /* post_task */}},
       {kConfigurationUrl2, {kConfigurationData2}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);
  SetConfiguration(kConfigurationUrl2, kConfigurationHash1);

  // The second request should not be sent, since the hash did not change.
  WaitForUpdate();
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}),
                              {{kKey1, kValue1}, {kKey2, kValue2}}));
}

IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPITest,
                       NonDictionaryConfiguration) {
  EnableTestServer({{kConfigurationUrl1, {kConfigurationData3}}});
  SetConfiguration(kConfigurationUrl1, kConfigurationHash1);

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(DictValueEquals(GetValues({kKey1, kKey2}), {}));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

// Test the API behavior in the Guest Session.
class ManagedConfigurationAPIGuestTest
    : public ManagedConfigurationAPITestBase {
 public:
  ManagedConfigurationAPIGuestTest() {
    // Suppress the InProcessBrowserTest's default behavior of opening
    // about://blank pages and let the standard startup code open the
    // chrome://newtab page. The reason is that the navigator.managed API
    // doesn't work on about://blank pages.
    set_open_about_blank_on_browser_launch(false);
  }

  ~ManagedConfigurationAPIGuestTest() override = default;
  ManagedConfigurationAPIGuestTest(const ManagedConfigurationAPIGuestTest&) =
      delete;
  ManagedConfigurationAPIGuestTest& operator=(
      const ManagedConfigurationAPIGuestTest&) = delete;

 protected:
  // Returns the result of navigator.managed.getManagedConfiguration().
  content::EvalJsResult GetValuesFromJsApi(
      const std::vector<std::string>& keys) {
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    if (!tab) {
      ADD_FAILURE() << "No tab active";
      return {base::Value(), std::string()};
    }
    base::Value::List keys_value;
    for (const auto& key : keys) {
      keys_value.Append(key);
    }
    return content::EvalJs(
        tab, content::JsReplace("navigator.managed.getManagedConfiguration($1)",
                                base::Value(std::move(keys_value))));
  }

 private:
  ash::GuestSessionMixin guest_session_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(ManagedConfigurationAPIGuestTest, Disabled) {
  EXPECT_EQ(api(), nullptr);
  // The JS API should return an error (but not cause a crash - it's a
  // regression test for b/231283325).
  EXPECT_THAT(
      GetValuesFromJsApi({kKey1}),
      testing::Field("error", &content::EvalJsResult::error,
                     Eq("a JavaScript error: \"NotAllowedError: Service "
                        "connection error. This API is available only for "
                        "managed apps.\"\n")));
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
