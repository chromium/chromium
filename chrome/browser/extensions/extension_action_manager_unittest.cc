// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_manager.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const char kBrowserAction[] = "browser_action";
const char kPageAction[] = "page_action";

}  // namespace

class ExtensionActionManagerTest : public testing::Test {
 public:
  ExtensionActionManagerTest();

 protected:
  // Build an extension, populating |action_type| key with |action|, and
  // "icons" key with |extension_icons|.
  scoped_refptr<const Extension> BuildExtension(
      std::unique_ptr<base::DictionaryValue> extension_icons,
      std::unique_ptr<base::DictionaryValue> action,
      const char* action_type);

  // Returns true if |action|'s title matches |extension|'s name.
  bool TitlesMatch(const Extension& extension, const ExtensionAction& action);

  // Returns true if |action|'s icon for size |action_key| matches
  // |extension|'s icon for size |extension_key|;
  bool IconsMatch(const Extension& extension,
                  int extension_key,
                  const ExtensionAction& action,
                  int action_key);

  // Returns the appropriate action for |extension| according to |action_type|.
  ExtensionAction* GetAction(const char* action_type,
                             const Extension& extension);

  // Tests that values that are missing from the |action_type| key are properly
  // populated with values from the other keys in the manifest (e.g.
  // "default_icon" key of |action_type| is populated with "icons" key).
  void TestPopulateMissingValues(const char* action_type);

  ExtensionActionManager* manager() { return manager_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  ExtensionRegistry* registry_;
  int curr_id_;
  ExtensionActionManager* manager_;
  std::unique_ptr<TestingProfile> profile_;
};

ExtensionActionManagerTest::ExtensionActionManagerTest()
    : curr_id_(0),
      profile_(new TestingProfile) {
  registry_ = ExtensionRegistry::Get(profile_.get());
  manager_ = ExtensionActionManager::Get(profile_.get());
}

scoped_refptr<const Extension> ExtensionActionManagerTest::BuildExtension(
    std::unique_ptr<base::DictionaryValue> extension_icons,
    std::unique_ptr<base::DictionaryValue> action,
    const char* action_type) {
  std::string id = base::IntToString(curr_id_++);
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(
              DictionaryBuilder()
                  .Set("version", "1")
                  .Set("manifest_version", 2)
                  .Set("icons", std::move(extension_icons))
                  .Set(action_type, std::move(action))
                  .Set("name", std::string("Test Extension").append(id))
                  .Build())
          .SetID(id)
          .Build();
  registry_->AddEnabled(extension);
  return extension;
}

bool ExtensionActionManagerTest::TitlesMatch(const Extension& extension,
                                             const ExtensionAction& action) {
  return action.GetTitle(ExtensionAction::kDefaultTabId) == extension.name();
}

bool ExtensionActionManagerTest::IconsMatch(const Extension& extension,
                                            int extension_key,
                                            const ExtensionAction& action,
                                            int action_key) {
  return action.default_icon()->Get(action_key,
                                    ExtensionIconSet::MATCH_BIGGER) ==
         IconsInfo::GetIcons(&extension)
             .Get(extension_key, ExtensionIconSet::MATCH_EXACTLY);
}

ExtensionAction* ExtensionActionManagerTest::GetAction(
    const char* action_type,
    const Extension& extension) {
  return (action_type == kBrowserAction) ?
      manager_->GetBrowserAction(extension) :
      manager_->GetPageAction(extension);
}

void ExtensionActionManagerTest::TestPopulateMissingValues(
    const char* action_type) {
  // Test that the largest icon from the extension's "icons" key is chosen as a
  // replacement for missing action default_icons keys. "19" should not be
  // replaced because "38" can always be used in its place.
  scoped_refptr<const Extension> extension =
      BuildExtension(DictionaryBuilder()
                         .Set("48", "icon48.png")
                         .Set("128", "icon128.png")
                         .Build(),
                     DictionaryBuilder().Build(), action_type);

  ASSERT_TRUE(extension.get());
  const ExtensionAction* action = GetAction(action_type, *extension);
  ASSERT_TRUE(action);

  ASSERT_TRUE(TitlesMatch(*extension, *action));
  ASSERT_TRUE(IconsMatch(*extension, 48, *action, 38));

  // Test that the action's missing default_icons are not replaced with smaller
  // icons.
  extension =
      BuildExtension(DictionaryBuilder().Set("24", "icon24.png").Build(),
                     DictionaryBuilder().Build(), action_type);

  ASSERT_TRUE(extension.get());
  action = GetAction(action_type, *extension);
  ASSERT_TRUE(action);

  ASSERT_TRUE(IconsMatch(*extension, 24, *action, 19));
  ASSERT_FALSE(IconsMatch(*extension, 24, *action, 38));

  // Test that an action's 19px icon is not replaced if a 38px action icon
  // exists.
  extension = BuildExtension(
      DictionaryBuilder().Set("128", "icon128.png").Build(),
      DictionaryBuilder()
          .Set("default_icon",
               DictionaryBuilder().Set("38", "action38.png").Build())
          .Build(),
      action_type);

  ASSERT_TRUE(extension.get());
  action = GetAction(action_type, *extension);
  ASSERT_TRUE(action);

  ASSERT_FALSE(IconsMatch(*extension, 128, *action, 19));

  // Test that existing default_icons and default_title are not replaced.
  extension =
      BuildExtension(DictionaryBuilder().Set("128", "icon128.png").Build(),
                     DictionaryBuilder()
                         .Set("default_title", "Action!")
                         .Set("default_icon", DictionaryBuilder()
                                                  .Set("19", "action19.png")
                                                  .Set("38", "action38.png")
                                                  .Build())
                         .Build(),
                     action_type);

  ASSERT_TRUE(extension.get());
  action = GetAction(action_type, *extension);
  ASSERT_TRUE(action);

  ASSERT_FALSE(TitlesMatch(*extension, *action));
  ASSERT_FALSE(IconsMatch(*extension, 128, *action, 19));
  ASSERT_FALSE(IconsMatch(*extension, 128, *action, 38));
}

namespace {

TEST_F(ExtensionActionManagerTest, PopulateBrowserAction) {
  TestPopulateMissingValues(kBrowserAction);
}

TEST_F(ExtensionActionManagerTest, PopulatePageAction) {
  TestPopulateMissingValues(kPageAction);
}

TEST_F(ExtensionActionManagerTest, GetBestFitActionTest) {
  // Create an extension with page action defaults.
  scoped_refptr<const Extension> extension = BuildExtension(
      DictionaryBuilder().Set("48", "icon48.png").Build(),
      DictionaryBuilder()
          .Set("default_title", "Action!")
          .Set("default_icon",
               DictionaryBuilder().Set("38", "action38.png").Build())
          .Build(),
      kPageAction);
  ASSERT_TRUE(extension.get());

  // Get a "best fit" browser action for |extension|.
  std::unique_ptr<ExtensionAction> action =
      manager()->GetBestFitAction(*extension, ActionInfo::TYPE_BROWSER);
  ASSERT_TRUE(action.get());
  ASSERT_EQ(action->action_type(), ActionInfo::TYPE_BROWSER);

  // |action|'s title and default icon should match |extension|'s page action's.
  ASSERT_EQ(action->GetTitle(ExtensionAction::kDefaultTabId), "Action!");
  ASSERT_EQ(action->default_icon()->Get(38, ExtensionIconSet::MATCH_EXACTLY),
            "action38.png");

  // Create a new extension without page action defaults.
  extension =
      BuildExtension(DictionaryBuilder().Set("48", "icon48.png").Build(),
                     DictionaryBuilder().Build(), kPageAction);
  ASSERT_TRUE(extension.get());

  action = manager()->GetBestFitAction(*extension, ActionInfo::TYPE_BROWSER);

  // Now these values match because |extension| does not have page action
  // defaults.
  ASSERT_TRUE(TitlesMatch(*extension, *action));
  ASSERT_TRUE(IconsMatch(*extension, 48, *action, 38));
}

}  // namespace
}  // namespace extensions
