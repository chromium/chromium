// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_tab_restore_helper.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_tab_restore_data.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

class MockInstance : public GlicInstanceHelper::Instance {
 public:
  MockInstance(InstanceId id, std::optional<std::string> conversation_id)
      : id_(std::move(id)), conversation_id_(std::move(conversation_id)) {}

  const InstanceId& id() const override { return id_; }
  std::optional<std::string> conversation_id() const override {
    return conversation_id_;
  }

 private:
  InstanceId id_;
  std::optional<std::string> conversation_id_;
};

class GlicTabRestoreHelperBrowserTest : public InProcessBrowserTest {
 public:
  GlicTabRestoreHelperBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kGlicTabRestoration);
  }

 private:
  GlicTestEnvironment glic_test_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicTabRestoreHelperBrowserTest, PopulateAndRestore) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  auto* tab = GetTabListInterface()->GetActiveTab();

  // 1. Setup Glic state on the tab
  GlicInstanceHelper* helper = GlicInstanceHelper::From(tab);
  ASSERT_TRUE(helper);

  std::string instance_id_str = "00000000-0000-0000-0000-000000000001";
  std::string conversation_id = "conversation1";
  MockInstance bound_instance(base::Uuid::ParseLowercase(instance_id_str),
                              conversation_id);
  helper->SetBoundInstance(&bound_instance);

  std::string pinned_instance_id_str = "00000000-0000-0000-0000-000000000002";
  MockInstance pinned_instance(
      base::Uuid::ParseLowercase(pinned_instance_id_str), std::nullopt);
  helper->OnPinnedByInstance(&pinned_instance);

  // 2. Populate extra_data
  std::map<std::string, std::string> extra_data;
  PopulateGlicExtraData(tab->GetContents(), &extra_data);

  // 3. Verify extra_data contents
  EXPECT_EQ(extra_data["glic.instance_id"], instance_id_str);
  EXPECT_EQ(extra_data["glic.conversation_id"], conversation_id);
  // Verify pinned instances JSON (simplified check)
  std::string pinned_json = extra_data["glic.pinned_instances"];
  EXPECT_THAT(pinned_json, testing::HasSubstr(pinned_instance_id_str));

  // 4. Restore state to a fresh WebContents
  std::unique_ptr<content::WebContents> restored_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile()));
  RestoreGlicStateFromExtraData(restored_contents.get(), extra_data);

  // 5. Verify restored data
  GlicTabRestoreData* restore_data =
      GlicTabRestoreData::FromWebContents(restored_contents.get());
  ASSERT_TRUE(restore_data);
  const GlicRestoredState& state = restore_data->state();

  EXPECT_EQ(state.bound_instance.instance_id, instance_id_str);
  EXPECT_EQ(state.bound_instance.conversation_id, conversation_id);
  ASSERT_EQ(state.pinned_instances.size(), 1u);
  EXPECT_EQ(state.pinned_instances[0].instance_id, pinned_instance_id_str);

  // Clean up to avoid dangling pointers since mock instances are on the stack.
  helper->SetBoundInstance(nullptr);
  helper->OnUnpinnedByInstance(&pinned_instance);
}

}  // namespace
}  // namespace glic
