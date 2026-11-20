// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bidi_mapper/host/bidi_mapper_host.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/json/json_writer.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/bidi_mapper/host/bidi_mapper_ui.h"
#include "components/bidi_mapper/host/features.h"
#include "components/policy/core/browser/developer_tools_availability.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using FailureReason = bidi_mapper::BidiMapperHost::FailureReason;

constexpr int32_t kPrivilegedFeatureId = 42;

// A client of a BidiMapperHost that sends BiDi commands and collects the
// responses to them.
class TestClient : public bidi_mapper::BidiMapperHost::Client {
 public:
  explicit TestClient(content::BrowserContext* browser_context)
      : host_(bidi_mapper::BidiMapperHost::Create(browser_context,
                                                  kPrivilegedFeatureId,
                                                  this)) {}

  bidi_mapper::BidiMapperHost* host() { return host_.get(); }

  bool WaitUntilStarted() { return !start_outcome_.Get().has_value(); }
  std::optional<FailureReason> WaitForFailure() { return start_outcome_.Get(); }

  // Sends a command and returns the response to it.
  base::DictValue SendCommand(std::string_view method,
                              std::string_view params = "{}") {
    const int id = ++last_command_id_;
    base::DictValue command;
    command.Set("id", id);
    command.Set("method", method);
    command.Set("params", base::test::ParseJsonDict(params));
    host_->SendBidiMessage(*base::WriteJson(command));
    if (!base::test::RunUntil(
            [&] { return responses_.contains(id) || is_closed_; }) ||
        !responses_.contains(id)) {
      ADD_FAILURE() << "No response to " << method;
      return base::DictValue();
    }
    return std::move(responses_.extract(id).mapped());
  }

  bool WaitUntilClosed() {
    return base::test::RunUntil([&] { return is_closed_; });
  }

 private:
  // bidi_mapper::BidiMapperHost::Client:
  void OnStarted() override { start_outcome_.SetValue(std::nullopt); }

  void OnFailed(FailureReason reason) override {
    start_outcome_.SetValue(reason);
  }

  void OnMessage(const std::string& message) override {
    base::DictValue dict = base::test::ParseJsonDict(message);
    // Events have no id.
    if (std::optional<int> id = dict.FindInt("id")) {
      responses_[*id] = std::move(dict);
    }
  }

  void OnClosed() override { is_closed_ = true; }

  std::unique_ptr<bidi_mapper::BidiMapperHost> host_;
  base::test::TestFuture<std::optional<FailureReason>> start_outcome_;
  int last_command_id_ = 0;
  std::map<int, base::DictValue> responses_;
  bool is_closed_ = false;
};

// Returns true if `response` is the response to a command that succeeded.
bool Succeeded(const base::DictValue& response) {
  const std::string* type = response.FindString("type");
  return type && *type == "success";
}

// Tests BidiMapperHost in the browser, which registers the mapper document,
// decides what DevTools clients may attach to, and creates the tabs that a
// BiDi session asks for.
class ChromeBidiMapperHostBrowserTest : public InProcessBrowserTest {
 public:
  ChromeBidiMapperHostBrowserTest() {
    feature_list_.InitAndEnableFeature(
        bidi_mapper::features::kChromeDriverExtensionApi);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeBidiMapperHostBrowserTest, DrivesBrowser) {
  ASSERT_TRUE(embedded_test_server()->Start());

  TestClient client(browser()->GetProfile());
  ASSERT_TRUE(client.host());
  ASSERT_TRUE(client.WaitUntilStarted());
  ASSERT_TRUE(
      Succeeded(client.SendCommand("session.new", R"({"capabilities":{}})")));

  // The session sees the tab of the browser, and not the mapper.
  base::DictValue tree = client.SendCommand("browsingContext.getTree");
  const base::ListValue* contexts =
      tree.FindListByDottedPath("result.contexts");
  ASSERT_TRUE(contexts) << tree;
  EXPECT_EQ(contexts->size(), 1u);

  // The session opens a tab in the browser and navigates it.
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 1);
  base::DictValue created =
      client.SendCommand("browsingContext.create", R"({"type":"tab"})");
  const std::string* context = created.FindStringByDottedPath("result.context");
  ASSERT_TRUE(context) << created;
  ASSERT_EQ(tab_strip_model->count(), 2);

  const GURL url = embedded_test_server()->GetURL("/title2.html");
  base::DictValue navigate_params;
  navigate_params.Set("context", *context);
  navigate_params.Set("url", url.spec());
  navigate_params.Set("wait", "complete");
  EXPECT_TRUE(Succeeded(client.SendCommand("browsingContext.navigate",
                                           *base::WriteJson(navigate_params))));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(1)->GetLastCommittedURL(), url);

  // The session sees where the tab is now.
  base::DictValue root_params;
  root_params.Set("root", *context);
  tree = client.SendCommand("browsingContext.getTree",
                            *base::WriteJson(root_params));
  contexts = tree.FindListByDottedPath("result.contexts");
  ASSERT_TRUE(contexts) << tree;
  ASSERT_EQ(contexts->size(), 1u);
  const std::string* context_url = (*contexts)[0].GetDict().FindString("url");
  ASSERT_TRUE(context_url) << tree;
  EXPECT_EQ(*context_url, url.spec());

  // Ending the session closes the host, and leaves the tab open.
  EXPECT_TRUE(Succeeded(client.SendCommand("session.end")));
  EXPECT_TRUE(client.WaitUntilClosed());
  EXPECT_EQ(tab_strip_model->count(), 2);
}

// The browser does not let DevTools clients attach when developer tools are
// disabled by policy. That applies to the host as well.
IN_PROC_BROWSER_TEST_F(ChromeBidiMapperHostBrowserTest, NotAllowedByPolicy) {
  browser()->GetProfile()->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(policy::DeveloperToolsAvailability::kDisallowed));

  TestClient client(browser()->GetProfile());
  ASSERT_TRUE(client.host());
  EXPECT_EQ(client.WaitForFailure(), FailureReason::kNotAllowed);
}

IN_PROC_BROWSER_TEST_F(ChromeBidiMapperHostBrowserTest, MapperDocumentExists) {
  EXPECT_TRUE(content::WebUIConfigMap::GetInstance().GetConfig(
      browser()->GetProfile(), GURL(bidi_mapper::kBidiMapperUIURL)));
}

using ChromeBidiMapperHostDisabledBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(ChromeBidiMapperHostDisabledBrowserTest, NothingExists) {
  Profile* profile = browser()->GetProfile();
  EXPECT_FALSE(bidi_mapper::BidiMapperHost::IsEnabled());
  TestClient client(profile);
  EXPECT_FALSE(client.host());
  EXPECT_FALSE(content::WebUIConfigMap::GetInstance().GetConfig(
      profile, GURL(bidi_mapper::kBidiMapperUIURL)));
}

}  // namespace
