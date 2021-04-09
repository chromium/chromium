// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_api/managed_configuration_api.h"

#include "chrome/browser/device_api/managed_configuration_api_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/components/policy/web_app_policy_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kOrigin[] = "https://example.com";
const char kConfigurationUrl1[] = "/conf1.json";
const char kConfigurationUrl2[] = "/conf2.json";
const char kConfigurationHash1[] = "asdas9jasidjd";
const char kConfigurationHash2[] = "ghi289sdfsdfk";
const char kConfigurationData1[] = R"(
{
  "key1": "value1",
  "key2" : 2
}
)";
const char kConfigurationData2[] = R"(
{
  "key1": "value_1",
  "key2" : "value_2"
}
)";
const char kKey1[] = "key1";
const char kKey2[] = "key2";
const char kKey3[] = "key3";
const char kValue1[] = "\"value1\"";
const char kValue2[] = "2";
const char kValue12[] = "\"value_1\"";
const char kValue22[] = "\"value_2\"";

struct ResponseTemplate {
  std::string response_body;
  bool should_post_task = false;
};

std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    std::map<std::string, ResponseTemplate> templates,
    const net::test_server::HttpRequest& request) {
  if (!base::Contains(templates, request.relative_url))
    return std::make_unique<net::test_server::HungResponse>();
  auto response_template = templates[request.relative_url];

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response;
  if (response_template.should_post_task) {
    http_response = std::make_unique<net::test_server::DelayedHttpResponse>(
        base::TimeDelta::FromSeconds(0));
  } else {
    http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  }

  http_response->set_code(net::HTTP_OK);
  http_response->set_content(response_template.response_body);
  http_response->set_content_type("text/plain");
  return http_response;
}

bool DictValueEquals(std::unique_ptr<base::DictionaryValue> value,
                     std::map<std::string, std::string> expected) {
  std::map<std::string, std::string> actual;
  for (const auto& entry : value->DictItems()) {
    if (!entry.second.is_string())
      return false;
    actual.insert({entry.first, entry.second.GetString()});
  }

  return actual == expected;
}

}  // namespace

class ManagedConfigurationAPITest : public InProcessBrowserTest,
                                    public ManagedConfigurationAPI::Observer {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    origin_ = url::Origin::Create(GURL(kOrigin));
    api()->AddObserver(this);
  }

  void TearDownOnMainThread() override {
    api()->RemoveObserver(this);
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void EnableTestServer(
      const std::map<std::string, ResponseTemplate> templates) {
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleRequest, templates));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetConfiguration(const std::string& conf_url,
                        const std::string& conf_hash) {
    auto trusted_apps = std::make_unique<base::ListValue>();
    auto entry = std::make_unique<base::DictionaryValue>();
    entry->SetString(ManagedConfigurationAPI::kOriginKey, kOrigin);
    entry->SetString(ManagedConfigurationAPI::kManagedConfigurationUrlKey,
                     embedded_test_server()->GetURL(conf_url).spec());
    entry->SetString(ManagedConfigurationAPI::kManagedConfigurationHashKey,
                     conf_hash);
    trusted_apps->Append(std::move(entry));
    profile()->GetPrefs()->Set(prefs::kManagedConfigurationPerOrigin,
                               *trusted_apps);
  }

  void ClearConfiguration() {
    profile()->GetPrefs()->Set(prefs::kManagedConfigurationPerOrigin,
                               base::ListValue());
  }

  void WaitForUpdate() {
    if (!updated_) {
      loop_update_ = std::make_unique<base::RunLoop>();
      loop_update_->Run();
    }
  }

  std::unique_ptr<base::DictionaryValue> GetValues(
      const std::vector<std::string>& keys) {
    updated_ = false;
    api()->GetOriginPolicyConfiguration(
        origin_, keys,
        base::BindOnce(&ManagedConfigurationAPITest::OnResultObtained,
                       base::Unretained(this)));

    // We could receive a failure asynchrounously.
    if (!updated_) {
      loop_get_ = std::make_unique<base::RunLoop>();
      loop_get_->Run();
      updated_ = false;
    }
    return std::move(result_);
  }

  void OnResultObtained(std::unique_ptr<base::DictionaryValue> result) {
    updated_ = true;
    result_ = std::move(result);
    loop_get_->Quit();
  }

  void OnManagedConfigurationChanged() override {
    if (loop_update_ && loop_update_->running()) {
      loop_update_->Quit();
      updated_ = false;
    } else {
      updated_ = true;
    }
  }

  const url::Origin& GetOrigin() override { return origin(); }

  ManagedConfigurationAPI* api() {
    return ManagedConfigurationAPIFactory::GetForProfile(profile());
  }

  Profile* profile() { return browser()->profile(); }
  const url::Origin& origin() { return origin_; }

 private:
  url::Origin origin_;

  bool updated_ = false;
  std::unique_ptr<base::RunLoop> loop_update_;
  std::unique_ptr<base::RunLoop> loop_get_;
  std::unique_ptr<base::DictionaryValue> result_;
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
  ASSERT_EQ(GetValues({kKey1, kKey2}), nullptr);
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
