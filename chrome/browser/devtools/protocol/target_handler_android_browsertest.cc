// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TargetHandlerAndroidIsolatedContextBrowserTest
    : public DevToolsProtocolTestBase {
 public:
  TargetHandlerAndroidIsolatedContextBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        chrome::android::kEnforceIncognitoIsolation);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TargetHandlerAndroidIsolatedContextBrowserTest,
                       CreateAndDisposeIsolatedBrowserContextTarget) {
  AttachToBrowserTarget();

  content::WebContents* initial_web_contents = web_contents();
  ASSERT_TRUE(initial_web_contents);
  TabAndroid* initial_tab = TabAndroid::FromWebContents(initial_web_contents);
  ASSERT_TRUE(initial_tab);
  TabModel* regular_model = TabModelList::GetTabModelForTabAndroid(initial_tab);
  ASSERT_TRUE(regular_model);
  const int initial_regular_tab_count = regular_model->GetTabCount();

  const base::DictValue* result =
      SendCommandSync("Target.createBrowserContext");
  ASSERT_TRUE(result);
  const std::string* browser_context_id =
      result->FindString("browserContextId");
  ASSERT_TRUE(browser_context_id);
  const std::string context_id = *browser_context_id;

  base::DictValue params;
  params.Set("url", "about:blank");
  params.Set("newWindow", true);
  params.Set("browserContextId", context_id);
  result = SendCommandSync("Target.createTarget", std::move(params));
  ASSERT_TRUE(result);
  const std::string* target_id_value = result->FindString("targetId");
  ASSERT_TRUE(target_id_value);
  const std::string target_id = *target_id_value;

  scoped_refptr<content::DevToolsAgentHost> agent_host =
      content::DevToolsAgentHost::GetForId(target_id);
  ASSERT_TRUE(agent_host);
  content::WebContents* otr_web_contents = agent_host->GetWebContents();
  ASSERT_TRUE(otr_web_contents);
  EXPECT_EQ(context_id, otr_web_contents->GetBrowserContext()->UniqueId());

  TabAndroid* otr_tab = TabAndroid::FromWebContents(otr_web_contents);
  ASSERT_TRUE(otr_tab);
  EXPECT_EQ(otr_web_contents->GetBrowserContext(), otr_tab->profile());

  TabModel* context_model = TabModelList::GetTabModelForTabAndroid(otr_tab);
  ASSERT_TRUE(context_model);
  EXPECT_TRUE(context_model->IsOffTheRecord());
  EXPECT_NE(regular_model, context_model);
  EXPECT_EQ(1, context_model->GetTabCount());
  EXPECT_EQ(initial_regular_tab_count, regular_model->GetTabCount());
  EXPECT_TRUE(TabModelList::Contains(context_model));

  // Default-profile targets continue to attach to the regular TabModel while
  // the isolated DevTools OTR TabModel is registered.
  params = base::DictValue();
  params.Set("url", "about:blank");
  params.Set("newWindow", false);
  result = SendCommandSync("Target.createTarget", std::move(params));
  ASSERT_TRUE(result);
  const std::string* default_target_id_value = result->FindString("targetId");
  ASSERT_TRUE(default_target_id_value);
  const std::string default_target_id = *default_target_id_value;

  scoped_refptr<content::DevToolsAgentHost> default_agent_host =
      content::DevToolsAgentHost::GetForId(default_target_id);
  ASSERT_TRUE(default_agent_host);
  content::WebContents* default_web_contents =
      default_agent_host->GetWebContents();
  ASSERT_TRUE(default_web_contents);
  TabAndroid* default_tab = TabAndroid::FromWebContents(default_web_contents);
  ASSERT_TRUE(default_tab);
  EXPECT_EQ(regular_model, TabModelList::GetTabModelForTabAndroid(default_tab));

  params = base::DictValue();
  params.Set("targetId", default_target_id);
  ASSERT_TRUE(SendCommandSync("Target.closeTarget", std::move(params)));

  // Disposing the browser context closes its tabs and unregisters the headless
  // OTR TabModel from TabModelList.
  params = base::DictValue();
  params.Set("browserContextId", context_id);
  ASSERT_TRUE(
      SendCommandSync("Target.disposeBrowserContext", std::move(params)));
  EXPECT_FALSE(agent_host->GetWebContents());
  EXPECT_FALSE(TabModelList::Contains(context_model));
}

}  // namespace
