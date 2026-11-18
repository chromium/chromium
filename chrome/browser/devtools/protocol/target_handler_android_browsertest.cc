// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/android/flags/chrome_cached_flags.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/incognito/android/incognito_utils_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TargetHandlerAndroidIsolatedContextBrowserTest
    : public DevToolsProtocolTestBase {
 public:
  TargetHandlerAndroidIsolatedContextBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        chrome::android::kEnforceIncognitoIsolation);
  }

  void SetUp() override {
    scoped_java_feature_.emplace(chrome::android::kEnforceIncognitoIsolation,
                                 true);
    DevToolsProtocolTestBase::SetUp();
  }

  void TearDown() override {
    DevToolsProtocolTestBase::TearDown();
    scoped_java_feature_.reset();
  }

 protected:
  size_t GetTabModelCount() {
    size_t count = 0;
    TabModelList::ForEachAll([&](TabModel*) {
      ++count;
      return true;
    });
    return count;
  }

  TabModel* GetInitialRegularModel() {
    content::WebContents* initial_web_contents = web_contents();
    if (!initial_web_contents) {
      return nullptr;
    }
    TabAndroid* initial_tab = TabAndroid::FromWebContents(initial_web_contents);
    return initial_tab ? TabModelList::GetTabModelForTabAndroid(initial_tab)
                       : nullptr;
  }

  std::string CreateBrowserContext() {
    const base::DictValue* result =
        SendCommandSync("Target.createBrowserContext");
    if (!result) {
      return "";
    }
    const std::string* browser_context_id =
        result->FindString("browserContextId");
    return browser_context_id ? *browser_context_id : "";
  }

  std::pair<std::string, scoped_refptr<content::DevToolsAgentHost>>
  CreateTarget(bool new_window,
               std::optional<std::string> browser_context_id = std::nullopt) {
    base::DictValue params;
    params.Set("url", "about:blank");
    params.Set("newWindow", new_window);
    if (browser_context_id.has_value()) {
      params.Set("browserContextId", *browser_context_id);
    }
    const base::DictValue* result =
        SendCommandSync("Target.createTarget", std::move(params));
    if (!result) {
      return {"", nullptr};
    }
    const std::string* target_id = result->FindString("targetId");
    if (!target_id) {
      return {"", nullptr};
    }
    return {*target_id, content::DevToolsAgentHost::GetForId(*target_id)};
  }

  void CloseTargetAndWait(const std::string& target_id,
                          content::WebContents* target_web_contents) {
    content::WebContentsDestroyedWatcher destroyed_watcher(target_web_contents);
    base::DictValue params;
    params.Set("targetId", target_id);
    ASSERT_TRUE(SendCommandSync("Target.closeTarget", std::move(params)));
    destroyed_watcher.Wait();
  }

  void DisposeBrowserContext(const std::string& context_id) {
    base::DictValue params;
    params.Set("browserContextId", context_id);
    ASSERT_TRUE(
        SendCommandSync("Target.disposeBrowserContext", std::move(params)));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::optional<chrome::android::ScopedJavaDrivenFeatureForTesting>
      scoped_java_feature_;
};

IN_PROC_BROWSER_TEST_F(TargetHandlerAndroidIsolatedContextBrowserTest,
                       CreateAndDisposeIsolatedBrowserContextTarget) {
  if (!chrome::android::ShouldOpenIncognitoAsWindow()) {
    GTEST_SKIP() << "Requires separate Incognito windows.";
  }

  AttachToBrowserTarget();

  TabModel* regular_model = GetInitialRegularModel();
  ASSERT_TRUE(regular_model);
  const int initial_regular_tab_count = regular_model->GetTabCount();
  const size_t initial_model_count = GetTabModelCount();

  const std::string context_id = CreateBrowserContext();
  ASSERT_FALSE(context_id.empty());
  // Creating an empty browser context must not eagerly register an OTR
  // TabModel in TabModelList.
  EXPECT_EQ(initial_model_count, GetTabModelCount());

  auto [first_target_id, first_agent_host] =
      CreateTarget(/*new_window=*/true, context_id);
  ASSERT_TRUE(first_agent_host);
  content::WebContents* first_otr_web_contents =
      first_agent_host->GetWebContents();
  ASSERT_TRUE(first_otr_web_contents);
  EXPECT_EQ(context_id,
            first_otr_web_contents->GetBrowserContext()->UniqueId());

  TabAndroid* first_otr_tab =
      TabAndroid::FromWebContents(first_otr_web_contents);
  ASSERT_TRUE(first_otr_tab);
  EXPECT_EQ(first_otr_web_contents->GetBrowserContext(),
            first_otr_tab->profile());
  EXPECT_TRUE(first_otr_tab->IsOffscreenRendering());
  ASSERT_TRUE(first_otr_web_contents->GetDelegate());
  EXPECT_EQ(nullptr,
            first_otr_web_contents->GetDelegate()->GetJavaScriptDialogManager(
                first_otr_web_contents));

  TabModel* context_model =
      TabModelList::GetTabModelForTabAndroid(first_otr_tab);
  ASSERT_TRUE(context_model);
  EXPECT_TRUE(context_model->IsOffTheRecord());
  EXPECT_FALSE(context_model->IsActiveModel());
  EXPECT_NE(regular_model, context_model);
  EXPECT_EQ(1, context_model->GetTabCount());
  EXPECT_EQ(initial_regular_tab_count, regular_model->GetTabCount());
  EXPECT_EQ(initial_model_count + 1, GetTabModelCount());

  // Closing the last isolated OTR target tears down the underlying
  // IncognitoTabModel delegate, and creating another target in the same
  // browser context recreates it cleanly.
  CloseTargetAndWait(first_target_id, first_otr_web_contents);
  EXPECT_EQ(initial_model_count, GetTabModelCount());

  auto [second_target_id, second_agent_host] =
      CreateTarget(/*new_window=*/false, context_id);
  ASSERT_TRUE(second_agent_host);
  content::WebContents* second_otr_web_contents =
      second_agent_host->GetWebContents();
  ASSERT_TRUE(second_otr_web_contents);
  EXPECT_EQ(initial_model_count + 1, GetTabModelCount());

  // Default-profile targets continue to attach to the regular TabModel while
  // the isolated DevTools OTR TabModel is registered.
  auto [default_target_id, default_agent_host] =
      CreateTarget(/*new_window=*/false);
  ASSERT_TRUE(default_agent_host);
  content::WebContents* default_web_contents =
      default_agent_host->GetWebContents();
  ASSERT_TRUE(default_web_contents);
  TabAndroid* default_tab = TabAndroid::FromWebContents(default_web_contents);
  ASSERT_TRUE(default_tab);
  EXPECT_EQ(regular_model, TabModelList::GetTabModelForTabAndroid(default_tab));

  CloseTargetAndWait(default_target_id, default_web_contents);

  // Disposing the browser context closes its tabs and unregisters the offscreen
  // OTR TabModel from TabModelList.
  content::WebContentsDestroyedWatcher second_destroyed_watcher(
      second_otr_web_contents);
  DisposeBrowserContext(context_id);
  second_destroyed_watcher.Wait();
  EXPECT_FALSE(second_agent_host->GetWebContents());
  EXPECT_EQ(initial_model_count, GetTabModelCount());
}

IN_PROC_BROWSER_TEST_F(TargetHandlerAndroidIsolatedContextBrowserTest,
                       CreateAndDisposeMixedWindowBrowserContextTarget) {
  if (chrome::android::ShouldOpenIncognitoAsWindow()) {
    GTEST_SKIP() << "Mixed-window Incognito TabModels are only used when "
                    "Incognito does not open in separate windows.";
  }

  AttachToBrowserTarget();

  TabModel* regular_model = GetInitialRegularModel();
  ASSERT_TRUE(regular_model);
  const int initial_regular_tab_count = regular_model->GetTabCount();
  const size_t initial_model_count = GetTabModelCount();

  const std::string context_id = CreateBrowserContext();
  ASSERT_FALSE(context_id.empty());
  EXPECT_EQ(initial_model_count, GetTabModelCount());

  // When ShouldOpenIncognitoAsWindow() is false, kEnforceIncognitoIsolation is
  // a no-op and isolated browser context targets continue to attach to the
  // mixed window's Incognito TabModel rather than an offscreen TabModel.
  auto [target_id, agent_host] = CreateTarget(/*new_window=*/true, context_id);
  ASSERT_TRUE(agent_host);
  content::WebContents* otr_web_contents = agent_host->GetWebContents();
  ASSERT_TRUE(otr_web_contents);
  EXPECT_EQ(context_id, otr_web_contents->GetBrowserContext()->UniqueId());

  TabAndroid* otr_tab = TabAndroid::FromWebContents(otr_web_contents);
  ASSERT_TRUE(otr_tab);
  EXPECT_EQ(otr_web_contents->GetBrowserContext(), otr_tab->profile());
  EXPECT_FALSE(otr_tab->IsOffscreenRendering());

  TabModel* context_model = TabModelList::GetTabModelForTabAndroid(otr_tab);
  ASSERT_TRUE(context_model);
  EXPECT_TRUE(context_model->IsOffTheRecord());
  EXPECT_NE(regular_model, context_model);
  EXPECT_EQ(1, context_model->GetTabCount());
  EXPECT_EQ(initial_regular_tab_count, regular_model->GetTabCount());
  EXPECT_EQ(initial_model_count + 1, GetTabModelCount());

  content::WebContentsDestroyedWatcher destroyed_watcher(otr_web_contents);
  DisposeBrowserContext(context_id);
  destroyed_watcher.Wait();
  EXPECT_FALSE(agent_host->GetWebContents());
  EXPECT_EQ(initial_model_count, GetTabModelCount());
}

}  // namespace
