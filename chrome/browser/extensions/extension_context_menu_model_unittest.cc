// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_context_menu_model.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/chrome_extension_browser_constants.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "chrome/browser/extensions/permissions_test_util.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/context_menus.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/test/browser_side_navigation_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/test/scoped_screen_override.h"
#include "ui/display/test/test_screen.h"
#include "ui/gfx/image/image.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#endif

namespace extensions {

using display::test::ScopedScreenOverride;

namespace {

void Increment(int* i) {
  CHECK(i);
  ++(*i);
}

// Label for test extension menu item.
const char* kTestExtensionItemLabel = "test-ext-item";

std::string item_label() {
  return kTestExtensionItemLabel;
}

class MenuBuilder {
 public:
  MenuBuilder(scoped_refptr<const Extension> extension,
              Browser* browser,
              MenuManager* menu_manager)
      : extension_(extension),
        browser_(browser),
        menu_manager_(menu_manager),
        cur_id_(0) {}
  ~MenuBuilder() {}

  std::unique_ptr<ExtensionContextMenuModel> BuildMenu() {
    return std::make_unique<ExtensionContextMenuModel>(
        extension_.get(), browser_, ExtensionContextMenuModel::VISIBLE, nullptr,
        true /* can_show_icon_in_toolbar */);
  }

  void AddContextItem(MenuItem::Context context) {
    MenuItem::Id id(false /* not incognito */,
                    MenuItem::ExtensionKey(extension_->id()));
    id.uid = ++cur_id_;
    menu_manager_->AddContextItem(
        extension_.get(),
        std::make_unique<MenuItem>(id, kTestExtensionItemLabel,
                                   false,  // check`ed
                                   true,   // visible
                                   true,   // enabled
                                   MenuItem::NORMAL,
                                   MenuItem::ContextList(context)));
  }

  void SetItemVisibility(int item_id, bool visible) {
    MenuItem::Id id(false /* not incognito */,
                    MenuItem::ExtensionKey(extension_->id()));
    id.uid = item_id;

    menu_manager_->GetItemById(id)->set_visible(visible);
  }

  void SetItemTitle(int item_id, const std::string& title) {
    MenuItem::Id id(false /* not incognito */,
                    MenuItem::ExtensionKey(extension_->id()));
    id.uid = item_id;

    menu_manager_->GetItemById(id)->set_title(title);
  }

 private:
  scoped_refptr<const Extension> extension_;
  Browser* browser_;
  MenuManager* menu_manager_;
  int cur_id_;

  DISALLOW_COPY_AND_ASSIGN(MenuBuilder);
};

// Returns the number of extension menu items that show up in |model|.
// For this test, all the extension items have same label
// |kTestExtensionItemLabel|.
int CountExtensionItems(const ExtensionContextMenuModel& model) {
  base::string16 expected_label = base::ASCIIToUTF16(kTestExtensionItemLabel);
  int num_items_found = 0;
  int num_custom_found = 0;
  for (int i = 0; i < model.GetItemCount(); ++i) {
    base::string16 actual_label = model.GetLabelAt(i);
    int command_id = model.GetCommandIdAt(i);
    // If the command id is not visible, it should not be counted.
    if (model.IsCommandIdVisible(command_id)) {
      // The last character of |expected_label| can be the item number (e.g
      // "test-ext-item" -> "test-ext-item1"). In checking that extensions items
      // have the same label |kTestExtensionItemLabel|, the specific item number
      // is ignored, [0, expected_label.size).
      if (base::StartsWith(actual_label, expected_label,
                           base::CompareCase::SENSITIVE))
        ++num_items_found;
      if (ContextMenuMatcher::IsExtensionsCustomCommandId(command_id))
        ++num_custom_found;
    }
  }
  // The only custom extension items present on the menu should be those we
  // added in the test.
  EXPECT_EQ(num_items_found, num_custom_found);
  return num_items_found;
}

// Checks that the model has the extension items in the exact order specified by
// |item_number|.
void VerifyItems(const ExtensionContextMenuModel& model,
                 std::vector<std::string> item_number) {
  size_t j = 0;
  for (int i = 0; i < model.GetItemCount(); i++) {
    int command_id = model.GetCommandIdAt(i);
    if (ContextMenuMatcher::IsExtensionsCustomCommandId(command_id) &&
        model.IsCommandIdVisible(command_id)) {
      ASSERT_LT(j, item_number.size());
      EXPECT_EQ(base::ASCIIToUTF16(item_label() + item_number[j]),
                model.GetLabelAt(i));
      j++;
    }
  }
  EXPECT_EQ(item_number.size(), j);
}

}  // namespace

class ExtensionContextMenuModelTest : public ExtensionServiceTestBase {
 public:
  enum class CommandState {
    kAbsent,    // The command is not present in the menu.
    kEnabled,   // The command is present, and enabled.
    kDisabled,  // The command is present, and disabled.
  };

  ExtensionContextMenuModelTest();

  // Build an extension to pass to the menu constructor, with the action
  // specified by |action_key|.
  const Extension* AddExtension(const std::string& name,
                                const char* action_key,
                                Manifest::Location location);
  const Extension* AddExtensionWithHostPermission(
      const std::string& name,
      const char* action_key,
      Manifest::Location location,
      const std::string& host_permission);
  // TODO(devlin): Consolidate this with the methods above.
  void InitializeAndAddExtension(const Extension& extension);

  Browser* GetBrowser();

  // Adds a new tab with |url| to the tab strip, and returns the WebContents
  // associated with it.
  content::WebContents* AddTab(const GURL& url);

  // Returns the current state for the specified page access |command|.
  CommandState GetPageAccessCommandState(const ExtensionContextMenuModel& menu,
                                         int command) const;

  // Returns true if the |menu| has the page access submenu at all.
  bool HasPageAccessSubmenu(const ExtensionContextMenuModel& menu) const;

  // Returns true if the |menu| has a valid entry for the "can't access page"
  // item.
  bool HasCantAccessPageEntry(const ExtensionContextMenuModel& menu) const;

  void SetUp() override;
  void TearDown() override;

 private:
  std::unique_ptr<TestBrowserWindow> test_window_;
  std::unique_ptr<Browser> browser_;
  display::test::TestScreen test_screen_;
  std::unique_ptr<ScopedScreenOverride> scoped_screen_override_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionContextMenuModelTest);
};

ExtensionContextMenuModelTest::ExtensionContextMenuModelTest() {}

const Extension* ExtensionContextMenuModelTest::AddExtension(
    const std::string& name,
    const char* action_key,
    Manifest::Location location) {
  return AddExtensionWithHostPermission(name, action_key, location,
                                        std::string());
}

const Extension* ExtensionContextMenuModelTest::AddExtensionWithHostPermission(
    const std::string& name,
    const char* action_key,
    Manifest::Location location,
    const std::string& host_permission) {
  DictionaryBuilder manifest;
  manifest.Set("name", name)
      .Set("version", "1")
      .Set("manifest_version", 2);
  if (action_key)
    manifest.Set(action_key, DictionaryBuilder().Build());
  if (!host_permission.empty())
    manifest.Set("permissions", ListBuilder().Append(host_permission).Build());
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(manifest.Build())
          .SetID(crx_file::id_util::GenerateId(name))
          .SetLocation(location)
          .Build();
  if (!extension.get())
    ADD_FAILURE();
  service()->GrantPermissions(extension.get());
  service()->AddExtension(extension.get());
  return extension.get();
}

void ExtensionContextMenuModelTest::InitializeAndAddExtension(
    const Extension& extension) {
  PermissionsUpdater updater(profile());
  updater.InitializePermissions(&extension);
  updater.GrantActivePermissions(&extension);
  service()->AddExtension(&extension);
}

Browser* ExtensionContextMenuModelTest::GetBrowser() {
  if (!browser_) {
    Browser::CreateParams params(profile(), true);
    test_window_.reset(new TestBrowserWindow());
    params.window = test_window_.get();
    browser_.reset(new Browser(params));
  }
  return browser_.get();
}

content::WebContents* ExtensionContextMenuModelTest::AddTab(const GURL& url) {
  std::unique_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  content::WebContents* raw_contents = contents.get();
  Browser* browser = GetBrowser();
  browser->tab_strip_model()->AppendWebContents(std::move(contents), true);
  EXPECT_EQ(browser->tab_strip_model()->GetActiveWebContents(), raw_contents);
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(raw_contents);
  web_contents_tester->NavigateAndCommit(url);
  return raw_contents;
}

ExtensionContextMenuModelTest::CommandState
ExtensionContextMenuModelTest::GetPageAccessCommandState(
    const ExtensionContextMenuModel& menu,
    int command) const {
  int submenu_index =
      menu.GetIndexOfCommandId(ExtensionContextMenuModel::PAGE_ACCESS_SUBMENU);
  if (submenu_index == -1)
    return CommandState::kAbsent;

  ui::MenuModel* submenu = menu.GetSubmenuModelAt(submenu_index);
  DCHECK(submenu);

  ui::MenuModel** menu_to_search = &submenu;
  int index_unused = 0;
  if (!ui::MenuModel::GetModelAndIndexForCommandId(command, menu_to_search,
                                                   &index_unused)) {
    return CommandState::kAbsent;
  }

  // The command is present; determine if it's enabled.
  return menu.IsCommandIdEnabled(command) ? CommandState::kEnabled
                                          : CommandState::kDisabled;
}

bool ExtensionContextMenuModelTest::HasPageAccessSubmenu(
    const ExtensionContextMenuModel& menu) const {
  return menu.GetIndexOfCommandId(
             ExtensionContextMenuModel::PAGE_ACCESS_SUBMENU) != -1;
}

bool ExtensionContextMenuModelTest::HasCantAccessPageEntry(
    const ExtensionContextMenuModel& menu) const {
  if (menu.GetIndexOfCommandId(
          ExtensionContextMenuModel::PAGE_ACCESS_CANT_ACCESS) == -1) {
    return false;
  }

  // The "Can't access this page" entry, if present, should always be disabled.
  EXPECT_FALSE(menu.IsCommandIdEnabled(
      ExtensionContextMenuModel::PAGE_ACCESS_CANT_ACCESS));
  return true;
}

void ExtensionContextMenuModelTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  content::BrowserSideNavigationSetUp();
  scoped_screen_override_ =
      std::make_unique<ScopedScreenOverride>(&test_screen_);
}

void ExtensionContextMenuModelTest::TearDown() {
  // Remove any tabs in the tab strip; else the test crashes.
  if (browser_) {
    while (!browser_->tab_strip_model()->empty())
      browser_->tab_strip_model()->DetachWebContentsAt(0);
  }

#if defined(OS_CHROMEOS)
  // The KioskAppManager, if initialized, needs to be cleaned up.
  // TODO(devlin): This should probably go somewhere more central, like
  // chromeos::ScopedCrosSettingsTestHelper.
  chromeos::KioskAppManager::Shutdown();
#endif

  content::BrowserSideNavigationTearDown();
  ExtensionServiceTestBase::TearDown();
}

// Tests that applicable menu items are disabled when a ManagementPolicy
// prohibits them.
TEST_F(ExtensionContextMenuModelTest, RequiredInstallationsDisablesItems) {
  InitializeEmptyExtensionService();

  // Test that management policy can determine whether or not policy-installed
  // extensions can be installed/uninstalled.
  const Extension* extension = AddExtension(
      "extension", manifest_keys::kPageAction, Manifest::EXTERNAL_POLICY);

  ExtensionContextMenuModel menu(extension, GetBrowser(),
                                 ExtensionContextMenuModel::VISIBLE, nullptr,
                                 true);

  ExtensionSystem* system = ExtensionSystem::Get(profile());
  system->management_policy()->UnregisterAllProviders();

  // Uninstallation should be, by default, enabled.
  EXPECT_TRUE(menu.IsCommandIdEnabled(ExtensionContextMenuModel::UNINSTALL));
  // Uninstallation should always be visible.
  EXPECT_TRUE(menu.IsCommandIdVisible(ExtensionContextMenuModel::UNINSTALL));

  TestManagementPolicyProvider policy_provider(
      TestManagementPolicyProvider::PROHIBIT_MODIFY_STATUS);
  system->management_policy()->RegisterProvider(&policy_provider);

  // If there's a policy provider that requires the extension stay enabled, then
  // uninstallation should be disabled.
  EXPECT_FALSE(menu.IsCommandIdEnabled(ExtensionContextMenuModel::UNINSTALL));
  int uninstall_index =
      menu.GetIndexOfCommandId(ExtensionContextMenuModel::UNINSTALL);
  // There should also be an icon to visually indicate why uninstallation is
  // forbidden.
  gfx::Image icon;
  EXPECT_TRUE(menu.GetIconAt(uninstall_index, &icon));
  EXPECT_FALSE(icon.IsEmpty());

  // Don't leave |policy_provider| dangling.
  system->management_policy()->UnregisterProvider(&policy_provider);
}

// Tests the context menu for a component extension.
TEST_F(ExtensionContextMenuModelTest, ComponentExtensionContextMenu) {
  InitializeEmptyExtensionService();

  std::string name("component");
  std::unique_ptr<base::DictionaryValue> manifest =
      DictionaryBuilder()
          .Set("name", name)
          .Set("version", "1")
          .Set("manifest_version", 2)
          .Set("browser_action", DictionaryBuilder().Build())
          .Build();

  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder()
            .SetManifest(base::WrapUnique(manifest->DeepCopy()))
            .SetID(crx_file::id_util::GenerateId("component"))
            .SetLocation(Manifest::COMPONENT)
            .Build();
    service()->AddExtension(extension.get());

    ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                   ExtensionContextMenuModel::VISIBLE, nullptr,
                                   true);

    // A component extension's context menu should not include options for
    // managing extensions or removing it, and should only include an option for
    // the options page if the extension has one (which this one doesn't).
    EXPECT_EQ(-1, menu.GetIndexOfCommandId(ExtensionContextMenuModel::OPTIONS));
    EXPECT_EQ(-1,
              menu.GetIndexOfCommandId(ExtensionContextMenuModel::UNINSTALL));
    EXPECT_EQ(-1, menu.GetIndexOfCommandId(
                      ExtensionContextMenuModel::MANAGE_EXTENSIONS));
    // The "name" option should be present, but not enabled for component
    // extensions.
    EXPECT_NE(-1,
              menu.GetIndexOfCommandId(ExtensionContextMenuModel::HOME_PAGE));
    EXPECT_FALSE(menu.IsCommandIdEnabled(ExtensionContextMenuModel::HOME_PAGE));
  }

  {
    // Check that a component extension with an options page does have the
    // options
    // menu item, and it is enabled.
    manifest->SetString("options_page", "options_page.html");
    scoped_refptr<const Extension> extension =
        ExtensionBuilder()
            .SetManifest(std::move(manifest))
            .SetID(crx_file::id_util::GenerateId("component_opts"))
            .SetLocation(Manifest::COMPONENT)
            .Build();
    ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                   ExtensionContextMenuModel::VISIBLE, nullptr,
                                   true);
    service()->AddExtension(extension.get());
    EXPECT_TRUE(extensions::OptionsPageInfo::HasOptionsPage(extension.get()));
    EXPECT_NE(-1, menu.GetIndexOfCommandId(ExtensionContextMenuModel::OPTIONS));
    EXPECT_TRUE(menu.IsCommandIdEnabled(ExtensionContextMenuModel::OPTIONS));
  }
}

TEST_F(ExtensionContextMenuModelTest, ExtensionItemTest) {
  InitializeEmptyExtensionService();
  const Extension* extension =
      AddExtension("extension", manifest_keys::kPageAction, Manifest::INTERNAL);

  // Create a MenuManager for adding context items.
  MenuManager* manager = static_cast<MenuManager*>(
      (MenuManagerFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(),
          base::BindRepeating(
              &MenuManagerFactory::BuildServiceInstanceForTesting))));
  ASSERT_TRUE(manager);

  MenuBuilder builder(extension, GetBrowser(), manager);

  // There should be no extension items yet.
  EXPECT_EQ(0, CountExtensionItems(*builder.BuildMenu()));

  // Add a browser action menu item.
  builder.AddContextItem(MenuItem::BROWSER_ACTION);
  // Since |extension| has a page action, the browser action menu item should
  // not be present.
  EXPECT_EQ(0, CountExtensionItems(*builder.BuildMenu()));

  // Add a page action menu item. This should be present because |extension|
  // has a page action.
  builder.AddContextItem(MenuItem::PAGE_ACTION);
  EXPECT_EQ(1, CountExtensionItems(*builder.BuildMenu()));

  // Create more page action items to test top-level menu item limitations.
  // We start at 1, so this should try to add the limit + 1.
  for (int i = 0; i < api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT; ++i)
    builder.AddContextItem(MenuItem::PAGE_ACTION);

  // We shouldn't go above the limit of top-level items.
  EXPECT_EQ(api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT,
            CountExtensionItems(*builder.BuildMenu()));
}

// Top-level extension actions, like 'browser_action' or 'page_action',
// are subject to an item limit chrome.contextMenus.ACTION_MENU_TOP_LEVEL_LIMIT.
// The test below ensures that:
//
// 1. The limit is respected for top-level items. In this case, we test
//    MenuItem::PAGE_ACTION.
// 2. Adding more items than the limit are ignored; only items within the limit
//    are visible.
// 3. Hiding items within the limit makes "extra" ones visible.
// 4. Unhiding an item within the limit hides a visible "extra" one.
TEST_F(ExtensionContextMenuModelTest,
       TestItemVisibilityAgainstItemLimitForTopLevelItems) {
  InitializeEmptyExtensionService();
  const Extension* extension =
      AddExtension("extension", manifest_keys::kPageAction, Manifest::INTERNAL);

  // Create a MenuManager for adding context items.
  MenuManager* manager = static_cast<MenuManager*>(
      MenuManagerFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(
                         &MenuManagerFactory::BuildServiceInstanceForTesting)));
  ASSERT_TRUE(manager);

  MenuBuilder builder(extension, GetBrowser(), manager);

  // There should be no extension items yet.
  EXPECT_EQ(0, CountExtensionItems(*builder.BuildMenu()));

  // Create more page action items to test top-level menu item limitations.
  for (int i = 1; i <= api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT; ++i) {
    builder.AddContextItem(MenuItem::PAGE_ACTION);
    builder.SetItemTitle(i, item_label().append(base::StringPrintf("%d", i)));
  }

  // We shouldn't go above the limit of top-level items.
  EXPECT_EQ(api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT,
            CountExtensionItems(*builder.BuildMenu()));

  // Add three more page actions items. This exceeds the top-level menu item
  // limit, so the three added should not be visible in the menu.
  builder.AddContextItem(MenuItem::PAGE_ACTION);
  builder.SetItemTitle(7, item_label() + "7");

  // By default, the additional page action items have their visibility set to
  // true. Test creating the eigth item such that it is hidden.
  builder.AddContextItem(MenuItem::PAGE_ACTION);
  builder.SetItemTitle(8, item_label() + "8");
  builder.SetItemVisibility(8, false);

  builder.AddContextItem(MenuItem::PAGE_ACTION);
  builder.SetItemTitle(9, item_label() + "9");

  std::unique_ptr<ExtensionContextMenuModel> model = builder.BuildMenu();

  // Ensure that the menu item limit is obeyed, meaning that the three
  // additional items are not visible in the menu.
  EXPECT_EQ(api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT,
            CountExtensionItems(*model));
  // Items 7 to 9 should not be visible in the model.
  VerifyItems(*model, {"1", "2", "3", "4", "5", "6"});

  // Hide the first two items.
  builder.SetItemVisibility(1, false);
  builder.SetItemVisibility(2, false);
  model = builder.BuildMenu();

  // Ensure that the menu item limit is obeyed.
  EXPECT_EQ(api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT,
            CountExtensionItems(*model));
  // Hiding the first two items in the model should make visible the "extra"
  // items -- items 7 and 9. Note, item 8 was set to hidden, so it should not
  // show in the model.
  VerifyItems(*model, {"3", "4", "5", "6", "7", "9"});

  // Unhide the eigth item.
  builder.SetItemVisibility(8, true);
  model = builder.BuildMenu();

  // Ensure that the menu item limit is obeyed.
  EXPECT_EQ(api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT,
            CountExtensionItems(*model));
  // The ninth item should be replaced with the eigth.
  VerifyItems(*model, {"3", "4", "5", "6", "7", "8"});

  // Unhide the first two items.
  builder.SetItemVisibility(1, true);
  builder.SetItemVisibility(2, true);
  model = builder.BuildMenu();

  EXPECT_EQ(api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT,
            CountExtensionItems(*model));
  // Unhiding the first two items should respect the menu item limit and
  // exclude the "extra" items -- items 7, 8, and 9 -- from the model.
  VerifyItems(*model, {"1", "2", "3", "4", "5", "6"});
}

// Tests that the standard menu items (e.g. uninstall, manage) are always
// visible.
TEST_F(ExtensionContextMenuModelTest,
       ExtensionContextMenuStandardItemsAlwaysVisible) {
  InitializeEmptyExtensionService();
  const Extension* extension =
      AddExtension("extension", manifest_keys::kPageAction, Manifest::INTERNAL);

  ExtensionContextMenuModel menu(extension, GetBrowser(),
                                 ExtensionContextMenuModel::VISIBLE, nullptr,
                                 true);
  EXPECT_TRUE(menu.IsCommandIdVisible(ExtensionContextMenuModel::HOME_PAGE));
  EXPECT_TRUE(menu.IsCommandIdVisible(ExtensionContextMenuModel::OPTIONS));
  EXPECT_TRUE(
      menu.IsCommandIdVisible(ExtensionContextMenuModel::TOGGLE_VISIBILITY));
  EXPECT_TRUE(menu.IsCommandIdVisible(ExtensionContextMenuModel::UNINSTALL));
  EXPECT_TRUE(
      menu.IsCommandIdVisible(ExtensionContextMenuModel::MANAGE_EXTENSIONS));
  EXPECT_TRUE(
      menu.IsCommandIdVisible(ExtensionContextMenuModel::INSPECT_POPUP));
  EXPECT_TRUE(menu.IsCommandIdVisible(
      ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_CLICK));
  EXPECT_TRUE(menu.IsCommandIdVisible(
      ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_SITE));
  EXPECT_TRUE(menu.IsCommandIdVisible(
      ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_ALL_SITES));
}

// Test that the "show" and "hide" menu items appear correctly in the extension
// context menu.
TEST_F(ExtensionContextMenuModelTest, ExtensionContextMenuShowAndHide) {
  InitializeEmptyExtensionService();
  Browser* browser = GetBrowser();
  extension_action_test_util::CreateToolbarModelForProfile(profile());
  const Extension* page_action =
      AddExtension("page_action_extension",
                     manifest_keys::kPageAction,
                     Manifest::INTERNAL);
  const Extension* browser_action =
      AddExtension("browser_action_extension",
                     manifest_keys::kBrowserAction,
                     Manifest::INTERNAL);

  // For laziness.
  const ExtensionContextMenuModel::MenuEntries visibility_command =
      ExtensionContextMenuModel::TOGGLE_VISIBILITY;
  base::string16 hide_string =
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_HIDE_BUTTON_IN_MENU);
  base::string16 show_string =
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_BUTTON_IN_TOOLBAR);
  base::string16 keep_string =
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_KEEP_BUTTON_IN_TOOLBAR);

  {
    // Even page actions should have a visibility option.
    ExtensionContextMenuModel menu(page_action, browser,
                                   ExtensionContextMenuModel::VISIBLE, nullptr,
                                   true);
    int index = menu.GetIndexOfCommandId(visibility_command);
    EXPECT_NE(-1, index);
    EXPECT_EQ(hide_string, menu.GetLabelAt(index));
  }

  {
    ExtensionContextMenuModel menu(browser_action, browser,
                                   ExtensionContextMenuModel::VISIBLE, nullptr,
                                   true);
    int index = menu.GetIndexOfCommandId(visibility_command);
    EXPECT_NE(-1, index);
    EXPECT_EQ(hide_string, menu.GetLabelAt(index));
    menu.ExecuteCommand(visibility_command, 0);
  }

  {
    // If the action is overflowed, it should have the "Show button in toolbar"
    // string.
    ExtensionContextMenuModel menu(browser_action, browser,
                                   ExtensionContextMenuModel::OVERFLOWED,
                                   nullptr, true);
    int index = menu.GetIndexOfCommandId(visibility_command);
    EXPECT_NE(-1, index);
    EXPECT_EQ(show_string, menu.GetLabelAt(index));
  }

  {
    // If the action is transitively visible, as happens when it is showing a
    // popup, we should use a "Keep button in toolbar" string.
    ExtensionContextMenuModel menu(
        browser_action, browser,
        ExtensionContextMenuModel::TRANSITIVELY_VISIBLE, nullptr, true);
    int index = menu.GetIndexOfCommandId(visibility_command);
    EXPECT_NE(-1, index);
    EXPECT_EQ(keep_string, menu.GetLabelAt(index));
  }
}

TEST_F(ExtensionContextMenuModelTest, ExtensionContextUninstall) {
  InitializeEmptyExtensionService();

  const Extension* extension = AddExtension(
      "extension", manifest_keys::kBrowserAction, Manifest::INTERNAL);
  const std::string extension_id = extension->id();
  ASSERT_TRUE(registry()->enabled_extensions().GetByID(extension_id));

  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);
  TestExtensionRegistryObserver uninstalled_observer(registry());
  {
    // Scope the menu so that it's destroyed during the uninstall process. This
    // reflects what normally happens (Chrome closes the menu when the uninstall
    // dialog shows up).
    ExtensionContextMenuModel menu(extension, GetBrowser(),
                                   ExtensionContextMenuModel::VISIBLE, nullptr,
                                   true);
    menu.ExecuteCommand(ExtensionContextMenuModel::UNINSTALL, 0);
  }
  uninstalled_observer.WaitForExtensionUninstalled();
  EXPECT_FALSE(registry()->GetExtensionById(extension_id,
                                            ExtensionRegistry::EVERYTHING));
}

TEST_F(ExtensionContextMenuModelTest, TestPageAccessSubmenu) {
  InitializeEmptyExtensionService();

  // Add an extension with all urls, and withhold permission.
  const Extension* extension =
      AddExtensionWithHostPermission("extension", manifest_keys::kBrowserAction,
                                     Manifest::INTERNAL, "*://*/*");
  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));

  const GURL kActiveUrl("http://www.example.com/");
  const GURL kOtherUrl("http://www.google.com/");

  // Add a tab to the browser.
  content::WebContents* web_contents = AddTab(kActiveUrl);

  ExtensionActionRunner* action_runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  ASSERT_TRUE(action_runner);

  // Pretend the extension wants to run.
  int run_count = 0;
  base::Closure increment_run_count(base::Bind(&Increment, &run_count));
  action_runner->RequestScriptInjectionForTesting(
      extension, UserScript::DOCUMENT_IDLE, increment_run_count);

  ExtensionContextMenuModel menu(extension, GetBrowser(),
                                 ExtensionContextMenuModel::VISIBLE, nullptr,
                                 true);

  EXPECT_NE(-1, menu.GetIndexOfCommandId(
                    ExtensionContextMenuModel::PAGE_ACCESS_SUBMENU));

  // For laziness.
  const ExtensionContextMenuModel::MenuEntries kRunOnClick =
      ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_CLICK;
  const ExtensionContextMenuModel::MenuEntries kRunOnSite =
      ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_SITE;
  const ExtensionContextMenuModel::MenuEntries kRunOnAllSites =
      ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_ALL_SITES;

  // Initial state: The extension should be in "run on click" mode.
  EXPECT_TRUE(menu.IsCommandIdChecked(kRunOnClick));
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnSite));
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnAllSites));

  // Initial state: The extension should have all permissions withheld, so
  // shouldn't be allowed to run on the active url or another arbitrary url, and
  // should have withheld permissions.
  ScriptingPermissionsModifier permissions_modifier(profile(), extension);
  EXPECT_FALSE(permissions_modifier.HasGrantedHostPermission(kActiveUrl));
  EXPECT_FALSE(permissions_modifier.HasGrantedHostPermission(kOtherUrl));
  const PermissionsData* permissions = extension->permissions_data();
  EXPECT_FALSE(permissions->withheld_permissions().IsEmpty());

  // Change the mode to be "Run on site".
  menu.ExecuteCommand(kRunOnSite, 0);
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnClick));
  EXPECT_TRUE(menu.IsCommandIdChecked(kRunOnSite));
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnAllSites));

  // The extension should have access to the active url, but not to another
  // arbitrary url, and the extension should still have withheld permissions.
  EXPECT_TRUE(permissions_modifier.HasGrantedHostPermission(kActiveUrl));
  EXPECT_FALSE(permissions_modifier.HasGrantedHostPermission(kOtherUrl));
  EXPECT_FALSE(permissions->withheld_permissions().IsEmpty());

  // Since the extension has permission, it should have ran.
  EXPECT_EQ(1, run_count);
  EXPECT_FALSE(action_runner->WantsToRun(extension));

  // On another url, the mode should still be run on click.
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents);
  web_contents_tester->NavigateAndCommit(kOtherUrl);
  EXPECT_TRUE(menu.IsCommandIdChecked(kRunOnClick));
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnSite));
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnAllSites));

  // And returning to the first url should return the mode to run on site.
  web_contents_tester->NavigateAndCommit(kActiveUrl);
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnClick));
  EXPECT_TRUE(menu.IsCommandIdChecked(kRunOnSite));
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnAllSites));

  // Request another run.
  action_runner->RequestScriptInjectionForTesting(
      extension, UserScript::DOCUMENT_IDLE, increment_run_count);

  // Change the mode to be "Run on all sites".
  menu.ExecuteCommand(kRunOnAllSites, 0);
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnClick));
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnSite));
  EXPECT_TRUE(menu.IsCommandIdChecked(kRunOnAllSites));

  // The extension should be able to run on any url, and shouldn't have any
  // withheld permissions.
  EXPECT_TRUE(permissions_modifier.HasGrantedHostPermission(kActiveUrl));
  EXPECT_TRUE(permissions_modifier.HasGrantedHostPermission(kOtherUrl));
  EXPECT_TRUE(permissions->withheld_permissions().IsEmpty());

  // It should have ran again.
  EXPECT_EQ(2, run_count);
  EXPECT_FALSE(action_runner->WantsToRun(extension));

  // On another url, the mode should also be run on all sites.
  web_contents_tester->NavigateAndCommit(kOtherUrl);
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnClick));
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnSite));
  EXPECT_TRUE(menu.IsCommandIdChecked(kRunOnAllSites));

  web_contents_tester->NavigateAndCommit(kActiveUrl);
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnClick));
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnSite));
  EXPECT_TRUE(menu.IsCommandIdChecked(kRunOnAllSites));

  action_runner->RequestScriptInjectionForTesting(
      extension, UserScript::DOCUMENT_IDLE, increment_run_count);

  // Return the mode to "Run on click".
  menu.ExecuteCommand(kRunOnClick, 0);
  EXPECT_TRUE(menu.IsCommandIdChecked(kRunOnClick));
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnSite));
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnAllSites));

  // We should return to the initial state - no access.
  EXPECT_FALSE(permissions_modifier.HasGrantedHostPermission(kActiveUrl));
  EXPECT_FALSE(permissions_modifier.HasGrantedHostPermission(kOtherUrl));
  EXPECT_FALSE(permissions->withheld_permissions().IsEmpty());

  // And the extension shouldn't have ran.
  EXPECT_EQ(2, run_count);
  EXPECT_TRUE(action_runner->WantsToRun(extension));

  // Install an extension requesting a single host. The page access submenu
  // should still be present.
  const Extension* single_host_extension = AddExtensionWithHostPermission(
      "single_host_extension", manifest_keys::kBrowserAction,
      Manifest::INTERNAL, "http://www.example.com/*");
  ExtensionContextMenuModel single_host_menu(
      single_host_extension, GetBrowser(), ExtensionContextMenuModel::VISIBLE,
      nullptr, true);
  EXPECT_NE(-1, single_host_menu.GetIndexOfCommandId(
                    ExtensionContextMenuModel::PAGE_ACCESS_SUBMENU));
}

TEST_F(ExtensionContextMenuModelTest, TestInspectPopupPresence) {
  InitializeEmptyExtensionService();
  {
    const Extension* page_action = AddExtension(
        "page_action", manifest_keys::kPageAction, Manifest::INTERNAL);
    ASSERT_TRUE(page_action);
    ExtensionContextMenuModel menu(page_action, GetBrowser(),
                                   ExtensionContextMenuModel::VISIBLE, nullptr,
                                   true);
    int inspect_popup_index =
        menu.GetIndexOfCommandId(ExtensionContextMenuModel::INSPECT_POPUP);
    EXPECT_GE(0, inspect_popup_index);
  }
  {
    const Extension* browser_action = AddExtension(
        "browser_action", manifest_keys::kBrowserAction, Manifest::INTERNAL);
    ExtensionContextMenuModel menu(browser_action, GetBrowser(),
                                   ExtensionContextMenuModel::VISIBLE, nullptr,
                                   true);
    int inspect_popup_index =
        menu.GetIndexOfCommandId(ExtensionContextMenuModel::INSPECT_POPUP);
    EXPECT_GE(0, inspect_popup_index);
  }
  {
    // An extension with no specified action has one synthesized. However,
    // there will never be a popup to inspect, so we shouldn't add a menu item.
    const Extension* no_action = AddExtension(
        "no_action", nullptr, Manifest::INTERNAL);
    ExtensionContextMenuModel menu(no_action, GetBrowser(),
                                   ExtensionContextMenuModel::VISIBLE, nullptr,
                                   true);
    int inspect_popup_index =
        menu.GetIndexOfCommandId(ExtensionContextMenuModel::INSPECT_POPUP);
    EXPECT_EQ(-1, inspect_popup_index);
  }
}

TEST_F(ExtensionContextMenuModelTest, PageAccessMenuOptions) {
  InitializeEmptyExtensionService();

  // For laziness.
  // TODO(devlin): Hoist these up to ExtensionContextMenuModelTest; enough
  // tests use them.
  using Entries = ExtensionContextMenuModel::MenuEntries;
  const Entries kOnClick = Entries::PAGE_ACCESS_RUN_ON_CLICK;
  const Entries kOnSite = Entries::PAGE_ACCESS_RUN_ON_SITE;
  const Entries kOnAllSites = Entries::PAGE_ACCESS_RUN_ON_ALL_SITES;
  const Entries kLearnMore = Entries::PAGE_ACCESS_LEARN_MORE;

  struct {
    // The pattern requested by the extension.
    std::string requested_pattern;
    // The pattern that's granted to the extension, if any. This may be
    // significantly different than the requested pattern.
    base::Optional<std::string> granted_pattern;
    // The current URL the context menu will be used on.
    GURL current_url;
    // The set of page access menu entries that should be present.
    std::set<Entries> expected_entries;
    // The set of page access menu entries that should be enabled.
    std::set<Entries> enabled_entries;
    // The selected page access menu entry.
    base::Optional<Entries> selected_entry;
  } test_cases[] = {
      // Easy cases: site the extension wants to run on, with or without
      // permission granted.
      {"https://google.com/maps",
       "https://google.com/maps",
       GURL("https://google.com/maps"),
       {kOnClick, kOnSite, kOnAllSites},
       {kOnClick, kOnSite},
       kOnSite},
      {"https://google.com/maps",
       base::nullopt,
       GURL("https://google.com/maps"),
       {kOnClick, kOnSite, kOnAllSites},
       {kOnClick, kOnSite},
       kOnClick},
      // We should display the page access controls if the extension wants to
      // run on the specified origin, even if not on the exact site itself.
      {"https://google.com/maps",
       "https://google.com/maps",
       GURL("https://google.com"),
       {kOnClick, kOnSite, kOnAllSites},
       {kOnClick, kOnSite},
       kOnSite},
      // The menu should be hidden if the extension cannot run on the origin.
      {"https://google.com/maps",
       "https://google.com/maps",
       GURL("https://mail.google.com"),
       {},
       {}},
      // An extension with all hosts granted should display the all sites
      // controls, even if it didn't request all sites.
      {"https://google.com/maps",
       "*://*/*",
       GURL("https://mail.google.com"),
       {kOnClick, kOnSite, kOnAllSites},
       {kOnClick, kOnSite, kOnAllSites},
       kOnAllSites},
      // Subdomain pattern tests.
      {"https://*.google.com/*",
       "https://*.google.com/*",
       GURL("https://google.com"),
       {kOnClick, kOnSite, kOnAllSites},
       {kOnClick, kOnSite},
       kOnSite},
      {"https://*.google.com/*",
       base::nullopt,
       GURL("https://google.com"),
       {kOnClick, kOnSite, kOnAllSites},
       {kOnClick, kOnSite},
       kOnClick},
      {"https://*.google.com/*",
       "https://*.google.com/*",
       GURL("https://mail.google.com"),
       {kOnClick, kOnSite, kOnAllSites},
       {kOnClick, kOnSite},
       kOnSite},
      {"https://*.google.com/*",
       "https://google.com/*",
       GURL("https://mail.google.com"),
       {kOnClick, kOnSite, kOnAllSites},
       {kOnClick, kOnSite},
       kOnClick},
      // On sites the extension doesn't want to run on, no controls should be
      // shown...
      {"https://*.google.com/*",
       base::nullopt,
       GURL("https://example.com"),
       {}},
      // ...unless the extension has access to the page, in which case we should
      // display the controls.
      {"https://*.google.com/*",
       "https://*.example.com/*",
       GURL("https://example.com"),
       {kOnClick, kOnSite, kOnAllSites},
       {kOnClick, kOnSite},
       kOnSite},
      // All-hosts like permissions should be treated as if the extension
      // requested access to all urls.
      {"https://*/maps",
       "https://*/maps",
       GURL("https://google.com/maps"),
       {kOnClick, kOnSite, kOnAllSites},
       {kOnClick, kOnSite, kOnAllSites},
       kOnAllSites},
      {"https://*/maps",
       "https://google.com/*",
       GURL("https://google.com/maps"),
       {kOnClick, kOnSite, kOnAllSites},
       {kOnClick, kOnSite, kOnAllSites},
       kOnSite},
      {"https://*/maps",
       "https://*/maps",
       GURL("https://google.com"),
       {kOnClick, kOnSite, kOnAllSites},
       {kOnClick, kOnSite, kOnAllSites},
       kOnAllSites},
      {"https://*/maps",
       "https://*/maps",
       GURL("https://chromium.org"),
       {kOnClick, kOnSite, kOnAllSites},
       {kOnClick, kOnSite, kOnAllSites},
       kOnAllSites},
      {"https://*.com/*",
       "https://*.com/*",
       GURL("https://google.com"),
       {kOnClick, kOnSite, kOnAllSites},
       {kOnClick, kOnSite, kOnAllSites},
       kOnAllSites},
      {"https://*.com/*",
       "https://*.com/*",
       GURL("https://maps.google.com"),
       {kOnClick, kOnSite, kOnAllSites},
       {kOnClick, kOnSite, kOnAllSites},
       kOnAllSites},
      // Even with an all-hosts like pattern, we shouldn't show access controls
      // if the extension can't run on the origin (though we show the learn more
      // option).
      {"https://*.com/*",
       "https://*.com/*",
       GURL("https://chromium.org"),
       {},
       {}},
      // No access controls should ever show for restricted pages, like
      // chrome:-scheme pages or the webstore.
      {"<all_urls>", "<all_urls>", GURL("chrome://extensions"), {}, {}},
      {"<all_urls>",
       "<all_urls>",
       ExtensionsClient::Get()->GetWebstoreBaseURL(),
       {},
       {}},
  };

  // Add a web contents to the browser.
  content::WebContents* web_contents = AddTab(GURL("about:blank"));
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents);

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf(
        "Request: '%s'; Granted: %s; URL: %s",
        test_case.requested_pattern.c_str(),
        test_case.granted_pattern.value_or(std::string()).c_str(),
        test_case.current_url.spec().c_str()));

    // Install an extension with the specified permission.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("test")
            .AddContentScript("script.js", {test_case.requested_pattern})
            .Build();
    InitializeAndAddExtension(*extension);

    ScriptingPermissionsModifier(profile(), extension)
        .SetWithholdHostPermissions(true);
    if (test_case.granted_pattern) {
      URLPattern pattern(UserScript::ValidUserScriptSchemes(false),
                         *test_case.granted_pattern);
      permissions_test_util::GrantRuntimePermissionsAndWaitForCompletion(
          profile(), *extension,
          PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                        URLPatternSet(), URLPatternSet({pattern})));
    }

    web_contents_tester->NavigateAndCommit(test_case.current_url);

    ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                   ExtensionContextMenuModel::VISIBLE, nullptr,
                                   true);

    EXPECT_EQ(test_case.selected_entry.has_value(),
              !test_case.expected_entries.empty())
        << "If any entries are available, one should be selected.";

    if (test_case.expected_entries.empty()) {
      // If there are no expected entries (i.e., the extension can't run on the
      // page), there should be no submenu and instead there should be a
      // disabled label.
      int label_index = menu.GetIndexOfCommandId(
          ExtensionContextMenuModel::PAGE_ACCESS_CANT_ACCESS);
      EXPECT_NE(-1, label_index);
      EXPECT_FALSE(menu.IsCommandIdEnabled(
          ExtensionContextMenuModel::PAGE_ACCESS_CANT_ACCESS));
      EXPECT_FALSE(HasPageAccessSubmenu(menu));
      continue;
    }

    // The learn more option should be visible whenever the menu is.
    EXPECT_EQ(CommandState::kEnabled,
              GetPageAccessCommandState(menu, kLearnMore));

    auto get_expected_state = [test_case](Entries command) {
      if (!test_case.expected_entries.count(command))
        return CommandState::kAbsent;
      return test_case.enabled_entries.count(command) ? CommandState::kEnabled
                                                      : CommandState::kDisabled;
    };

    // Verify the menu options are what we expect.
    EXPECT_EQ(get_expected_state(kOnClick),
              GetPageAccessCommandState(menu, kOnClick));
    EXPECT_EQ(get_expected_state(kOnSite),
              GetPageAccessCommandState(menu, kOnSite));
    EXPECT_EQ(get_expected_state(kOnAllSites),
              GetPageAccessCommandState(menu, kOnAllSites));

    auto should_command_be_checked = [test_case](int command) {
      return test_case.selected_entry && *test_case.selected_entry == command;
    };

    if (test_case.expected_entries.count(kOnClick)) {
      EXPECT_EQ(should_command_be_checked(kOnClick),
                menu.IsCommandIdChecked(kOnClick));
    }
    if (test_case.expected_entries.count(kOnSite)) {
      EXPECT_EQ(should_command_be_checked(kOnSite),
                menu.IsCommandIdChecked(kOnSite));
    }
    if (test_case.expected_entries.count(kOnAllSites)) {
      EXPECT_EQ(should_command_be_checked(kOnAllSites),
                menu.IsCommandIdChecked(kOnAllSites));
    }

    // Uninstall the extension so as not to conflict with more additions.
    base::string16 error;
    EXPECT_TRUE(service()->UninstallExtension(
        extension->id(), UNINSTALL_REASON_FOR_TESTING, &error));
    EXPECT_TRUE(error.empty()) << error;
    EXPECT_EQ(nullptr, registry()->GetInstalledExtension(extension->id()));
  }
}

TEST_F(ExtensionContextMenuModelTest, PageAccessWithActiveTab) {
  InitializeEmptyExtensionService();

  // For laziness.
  using Entries = ExtensionContextMenuModel::MenuEntries;
  const Entries kOnClick = Entries::PAGE_ACCESS_RUN_ON_CLICK;
  const Entries kOnSite = Entries::PAGE_ACCESS_RUN_ON_SITE;
  const Entries kOnAllSites = Entries::PAGE_ACCESS_RUN_ON_ALL_SITES;
  const Entries kLearnMore = Entries::PAGE_ACCESS_LEARN_MORE;

  // Add an extension that has activeTab. Note: we add permission for b.com so
  // that the extension is seen as affectable by the runtime host permissions
  // feature; otherwise the page access menu entry is omitted entirely.
  // TODO(devlin): Should we change that?
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddPermissions({"activeTab", "http://b.com/*"})
          .Build();
  InitializeAndAddExtension(*extension);

  AddTab(GURL("https://a.com"));

  ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                 ExtensionContextMenuModel::VISIBLE, nullptr,
                                 true);
  EXPECT_EQ(CommandState::kEnabled, GetPageAccessCommandState(menu, kOnClick));
  EXPECT_EQ(CommandState::kDisabled, GetPageAccessCommandState(menu, kOnSite));
  EXPECT_EQ(CommandState::kDisabled,
            GetPageAccessCommandState(menu, kOnAllSites));
  EXPECT_EQ(CommandState::kEnabled,
            GetPageAccessCommandState(menu, kLearnMore));
}

TEST_F(ExtensionContextMenuModelTest,
       TestTogglingAccessWithSpecificSitesWithUnrequestedUrl) {
  InitializeEmptyExtensionService();

  // For laziness.
  using Entries = ExtensionContextMenuModel::MenuEntries;
  const Entries kOnClick = Entries::PAGE_ACCESS_RUN_ON_CLICK;
  const Entries kOnSite = Entries::PAGE_ACCESS_RUN_ON_SITE;
  const Entries kOnAllSites = Entries::PAGE_ACCESS_RUN_ON_ALL_SITES;
  const Entries kLearnMore = Entries::PAGE_ACCESS_LEARN_MORE;

  // Add an extension that wants access to a.com.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").AddPermission("*://a.com/*").Build();
  InitializeAndAddExtension(*extension);

  // Additionally, grant it the (unrequested) access to b.com.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  URLPattern b_com_pattern(Extension::kValidHostPermissionSchemes,
                           "*://b.com/*");
  PermissionSet b_com_permissions(APIPermissionSet(), ManifestPermissionSet(),
                                  URLPatternSet({b_com_pattern}),
                                  URLPatternSet());
  prefs->AddGrantedPermissions(extension->id(), b_com_permissions);

  ScriptingPermissionsModifier modifier(profile(), extension);
  EXPECT_FALSE(modifier.HasWithheldHostPermissions());

  const GURL a_com("https://a.com");
  content::WebContents* web_contents = AddTab(a_com);

  {
    ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                   ExtensionContextMenuModel::VISIBLE, nullptr,
                                   true);

    // Without withholding host permissions, the menu should be visible on
    // a.com...
    EXPECT_TRUE(HasPageAccessSubmenu(menu));
    EXPECT_FALSE(HasCantAccessPageEntry(menu));

    EXPECT_EQ(CommandState::kEnabled,
              GetPageAccessCommandState(menu, kOnClick));
    EXPECT_EQ(CommandState::kEnabled, GetPageAccessCommandState(menu, kOnSite));
    EXPECT_EQ(CommandState::kDisabled,
              GetPageAccessCommandState(menu, kOnAllSites));
    EXPECT_EQ(CommandState::kEnabled,
              GetPageAccessCommandState(menu, kLearnMore));

    EXPECT_TRUE(menu.IsCommandIdChecked(kOnSite));
    EXPECT_FALSE(menu.IsCommandIdChecked(kOnClick));
    EXPECT_FALSE(menu.IsCommandIdChecked(kOnAllSites));
  }

  const GURL b_com("https://b.com");
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents);
  web_contents_tester->NavigateAndCommit(b_com);

  {
    // ... but not on b.com, where it doesn't want to run.
    ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                   ExtensionContextMenuModel::VISIBLE, nullptr,
                                   true);
    EXPECT_FALSE(HasPageAccessSubmenu(menu));
    EXPECT_TRUE(HasCantAccessPageEntry(menu));
  }

  modifier.SetWithholdHostPermissions(true);

  // However, if the extension has runtime-granted permissions to b.com, we
  // *should* display them in the menu.
  permissions_test_util::GrantRuntimePermissionsAndWaitForCompletion(
      profile(), *extension, b_com_permissions);

  {
    ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                   ExtensionContextMenuModel::VISIBLE, nullptr,
                                   true);
    EXPECT_TRUE(HasPageAccessSubmenu(menu));
    EXPECT_FALSE(HasCantAccessPageEntry(menu));
    EXPECT_EQ(CommandState::kEnabled,
              GetPageAccessCommandState(menu, kOnClick));
    EXPECT_EQ(CommandState::kEnabled, GetPageAccessCommandState(menu, kOnSite));
    EXPECT_EQ(CommandState::kDisabled,
              GetPageAccessCommandState(menu, kOnAllSites));
    EXPECT_EQ(CommandState::kEnabled,
              GetPageAccessCommandState(menu, kLearnMore));

    EXPECT_TRUE(menu.IsCommandIdChecked(kOnSite));
    EXPECT_FALSE(menu.IsCommandIdChecked(kOnClick));

    // Set the extension to run on click. This revokes b.com permissions.
    menu.ExecuteCommand(kOnClick, 0);
  }

  {
    ScriptingPermissionsModifier::SiteAccess site_access =
        modifier.GetSiteAccess(b_com);
    EXPECT_FALSE(site_access.has_site_access);
    EXPECT_FALSE(site_access.withheld_site_access);
  }

  ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                 ExtensionContextMenuModel::VISIBLE, nullptr,
                                 true);
  // Somewhat strangely, this also removes the access controls, because we don't
  // show it for sites the extension doesn't want to run on.
  EXPECT_FALSE(HasPageAccessSubmenu(menu));
  EXPECT_TRUE(HasCantAccessPageEntry(menu));
}

TEST_F(ExtensionContextMenuModelTest,
       TestTogglingAccessWithSpecificSitesWithRequestedSites) {
  InitializeEmptyExtensionService();

  // For laziness.
  using Entries = ExtensionContextMenuModel::MenuEntries;
  const Entries kOnClick = Entries::PAGE_ACCESS_RUN_ON_CLICK;
  const Entries kOnSite = Entries::PAGE_ACCESS_RUN_ON_SITE;
  const Entries kOnAllSites = Entries::PAGE_ACCESS_RUN_ON_ALL_SITES;

  // Add an extension that wants access to a.com and b.com.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddPermissions({"*://a.com/*", "*://b.com/*"})
          .Build();
  InitializeAndAddExtension(*extension);

  ScriptingPermissionsModifier modifier(profile(), extension);
  EXPECT_FALSE(modifier.HasWithheldHostPermissions());

  const GURL a_com("https://a.com");
  AddTab(a_com);

  ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                 ExtensionContextMenuModel::VISIBLE, nullptr,
                                 true);

  EXPECT_EQ(CommandState::kEnabled, GetPageAccessCommandState(menu, kOnClick));
  EXPECT_EQ(CommandState::kEnabled, GetPageAccessCommandState(menu, kOnSite));
  EXPECT_EQ(CommandState::kDisabled,
            GetPageAccessCommandState(menu, kOnAllSites));

  EXPECT_TRUE(menu.IsCommandIdChecked(kOnSite));
  EXPECT_FALSE(menu.IsCommandIdChecked(kOnClick));

  // Withhold access on a.com by setting the extension to on-click.
  menu.ExecuteCommand(kOnClick, 0);

  // This, sadly, removes access for the extension on b.com as well. :( This
  // is because we revoke all host permissions when transitioning from "don't
  // withhold" to "do withhold".
  // TODO(devlin): We should fix that, so that toggling access on a.com doesn't
  // revoke access on b.com.
  const GURL b_com("https://b.com");
  ScriptingPermissionsModifier::SiteAccess site_access =
      modifier.GetSiteAccess(b_com);
  EXPECT_FALSE(site_access.has_site_access);
  EXPECT_TRUE(site_access.withheld_site_access);
}

TEST_F(ExtensionContextMenuModelTest, TestClickingPageAccessLearnMore) {
  InitializeEmptyExtensionService();

  // Add an extension that wants access to a.com.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").AddPermission("*://a.com/*").Build();
  InitializeAndAddExtension(*extension);

  ScriptingPermissionsModifier modifier(profile(), extension);
  EXPECT_FALSE(modifier.HasWithheldHostPermissions());

  const GURL a_com("https://a.com");
  AddTab(a_com);

  Browser* browser = GetBrowser();
  ExtensionContextMenuModel menu(extension.get(), browser,
                                 ExtensionContextMenuModel::VISIBLE, nullptr,
                                 true);

  const ExtensionContextMenuModel::MenuEntries kLearnMore =
      ExtensionContextMenuModel::PAGE_ACCESS_LEARN_MORE;
  EXPECT_EQ(CommandState::kEnabled,
            GetPageAccessCommandState(menu, kLearnMore));
  menu.ExecuteCommand(kLearnMore, 0);

  EXPECT_EQ(2, browser->tab_strip_model()->count());
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  // Test web contents need a poke to commit.
  content::NavigationController& controller = web_contents->GetController();
  content::RenderFrameHostTester::CommitPendingLoad(&controller);

  EXPECT_EQ(GURL(chrome_extension_constants::kRuntimeHostPermissionsHelpURL),
            web_contents->GetLastCommittedURL());
}

TEST_F(ExtensionContextMenuModelTest, HistogramTest_Basic) {
  InitializeEmptyExtensionService();
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").Build();
  InitializeAndAddExtension(*extension);
  constexpr char kHistogramName[] = "Extensions.ContextMenuAction";
  {
    base::HistogramTester tester;
    {
      // The menu is constructed, but never shown.
      ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                     ExtensionContextMenuModel::VISIBLE,
                                     nullptr, true);
    }
    tester.ExpectTotalCount(kHistogramName, 0);
  }

  {
    base::HistogramTester tester;
    {
      // The menu is constructed and shown, but no action is taken.
      ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                     ExtensionContextMenuModel::VISIBLE,
                                     nullptr, true);
      menu.OnMenuWillShow(&menu);
      menu.MenuClosed(&menu);
    }
    tester.ExpectUniqueSample(
        kHistogramName, ExtensionContextMenuModel::ContextMenuAction::kNoAction,
        1 /* expected_count */);
  }

  {
    base::HistogramTester tester;
    {
      // The menu is constructed, shown, and an action taken.
      ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                     ExtensionContextMenuModel::VISIBLE,
                                     nullptr, true);
      menu.OnMenuWillShow(&menu);
      menu.ExecuteCommand(ExtensionContextMenuModel::MANAGE_EXTENSIONS, 0);
      menu.MenuClosed(&menu);
    }

    tester.ExpectUniqueSample(
        kHistogramName,
        ExtensionContextMenuModel::ContextMenuAction::kManageExtensions,
        1 /* expected_count */);
  }
}

TEST_F(ExtensionContextMenuModelTest, HistogramTest_CustomCommand) {
  constexpr char kHistogramName[] = "Extensions.ContextMenuAction";

  InitializeEmptyExtensionService();
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .SetAction(ExtensionBuilder::ActionType::BROWSER_ACTION)
          .Build();
  InitializeAndAddExtension(*extension);

  // Create a MenuManager for adding context items.
  MenuManager* manager = static_cast<MenuManager*>(
      (MenuManagerFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(),
          base::BindRepeating(
              &MenuManagerFactory::BuildServiceInstanceForTesting))));
  ASSERT_TRUE(manager);

  MenuBuilder builder(extension, GetBrowser(), manager);
  builder.AddContextItem(MenuItem::BROWSER_ACTION);
  std::unique_ptr<ExtensionContextMenuModel> menu = builder.BuildMenu();
  EXPECT_EQ(1, CountExtensionItems(*menu));

  base::HistogramTester tester;
  menu->OnMenuWillShow(menu.get());
  menu->ExecuteCommand(
      ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0), 0);
  menu->MenuClosed(menu.get());

  tester.ExpectUniqueSample(
      kHistogramName,
      ExtensionContextMenuModel::ContextMenuAction::kCustomCommand,
      1 /* expected_count */);
}

TEST_F(ExtensionContextMenuModelTest, HideToggleVisibility) {
  InitializeEmptyExtensionService();
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").Build();
  InitializeAndAddExtension(*extension);
  {
    ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                   ExtensionContextMenuModel::VISIBLE, nullptr,
                                   true /* can_show_icon_in_toolbar */);
    EXPECT_TRUE(
        menu.IsCommandIdVisible(ExtensionContextMenuModel::TOGGLE_VISIBILITY));
  }
  {
    ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                   ExtensionContextMenuModel::VISIBLE, nullptr,
                                   false /* can_show_icon_in_toolbar */);
    EXPECT_FALSE(
        menu.IsCommandIdVisible(ExtensionContextMenuModel::TOGGLE_VISIBILITY));
  }
}

}  // namespace extensions
