// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_context_menu_model.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "chrome/browser/extensions/permissions/permissions_test_util.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/extensions/permissions_url_constants.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/common/extensions/api/context_menus.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/api/extension_action/action_info_test_util.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/test/permissions_manager_waiter.h"
#include "net/disk_cache/blockfile/disk_format_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/test/test_screen.h"
#include "ui/gfx/image/image.h"
#include "url/origin.h"

namespace extensions {

using mojom::ManifestLocation;
using ContextMenuSource = ExtensionContextMenuModel::ContextMenuSource;
using MenuEntries = ExtensionContextMenuModel::MenuEntries;

const MenuEntries kGrantAllExtensions =
    ExtensionContextMenuModel::PAGE_ACCESS_ALL_EXTENSIONS_GRANTED;
const MenuEntries kBlockAllExtensions =
    ExtensionContextMenuModel::PAGE_ACCESS_ALL_EXTENSIONS_BLOCKED;
const MenuEntries kPageAccessSubmenu =
    ExtensionContextMenuModel::PAGE_ACCESS_SUBMENU;
const MenuEntries kOnClick =
    ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_CLICK;
const MenuEntries kOnSite = ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_SITE;
const MenuEntries kOnAllSites =
    ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_ALL_SITES;
const MenuEntries kPermissionsPage =
    ExtensionContextMenuModel::PAGE_ACCESS_PERMISSIONS_PAGE;
const MenuEntries kLearnMore =
    ExtensionContextMenuModel::PAGE_ACCESS_LEARN_MORE;
const MenuEntries kUninstall = ExtensionContextMenuModel::UNINSTALL;
const MenuEntries kPolicyInstalled =
    ExtensionContextMenuModel::POLICY_INSTALLED;

namespace {

void Increment(int* i, bool granted) {
  if (!granted)
    return;
  CHECK(i);
  ++(*i);
}

MenuItem::Context MenuItemContextForActionType(ActionInfo::Type type) {
  MenuItem::Context context = MenuItem::ALL;
  switch (type) {
    case ActionInfo::Type::kBrowser:
      context = MenuItem::BROWSER_ACTION;
      break;
    case ActionInfo::Type::kPage:
      context = MenuItem::PAGE_ACTION;
      break;
    case ActionInfo::Type::kAction:
      context = MenuItem::ACTION;
      break;
  }

  return context;
}

scoped_refptr<const Extension> BuildExtensionWithActionType(
    ActionInfo::Type type) {
  return ExtensionBuilder("extension")
      .SetAction(type)
      .SetManifestVersion(GetManifestVersionForActionType(type))
      .Build();
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

  MenuBuilder(const MenuBuilder&) = delete;
  MenuBuilder& operator=(const MenuBuilder&) = delete;

  ~MenuBuilder() {}

  std::unique_ptr<ExtensionContextMenuModel> BuildMenu() {
    return std::make_unique<ExtensionContextMenuModel>(
        extension_.get(), browser_, /*is_pinned=*/true, nullptr,
        /* can_show_icon_in_toolbar=*/true, ContextMenuSource::kToolbarAction);
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
  raw_ptr<Browser> browser_;
  raw_ptr<MenuManager> menu_manager_;
  int cur_id_;
};

// Returns the number of extension menu items that show up in |model|.
// For this test, all the extension items have same label
// |kTestExtensionItemLabel|.
int CountExtensionItems(const ExtensionContextMenuModel& model) {
  std::u16string expected_label = base::ASCIIToUTF16(kTestExtensionItemLabel);
  int num_items_found = 0;
  int num_custom_found = 0;
  for (size_t i = 0; i < model.GetItemCount(); ++i) {
    std::u16string actual_label = model.GetLabelAt(i);
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
  for (size_t i = 0; i < model.GetItemCount(); i++) {
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

  ExtensionContextMenuModelTest(const ExtensionContextMenuModelTest&) = delete;
  ExtensionContextMenuModelTest& operator=(
      const ExtensionContextMenuModelTest&) = delete;

  // Build an extension to pass to the menu constructor, with the action
  // specified by |action_key|.
  const Extension* AddExtension(const std::string& name,
                                const char* action_key,
                                ManifestLocation location);
  const Extension* AddExtensionWithHostPermission(
      const std::string& name,
      const char* action_key,
      ManifestLocation location,
      const std::string& host_permission);
  // TODO(devlin): Consolidate this with the methods above.
  void InitializeAndAddExtension(const Extension& extension);

  Browser* GetBrowser();

  MenuManager* CreateMenuManager();

  // Adds a new tab with |url| to the tab strip, and returns the WebContents
  // associated with it.
  content::WebContents* AddTab(const GURL& url);

  // Returns the current state for the specified `command` in `menu`.
  CommandState GetCommandState(const ExtensionContextMenuModel& menu,
                               int command) const;

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
};

ExtensionContextMenuModelTest::ExtensionContextMenuModelTest() {}

const Extension* ExtensionContextMenuModelTest::AddExtension(
    const std::string& name,
    const char* action_key,
    ManifestLocation location) {
  return AddExtensionWithHostPermission(name, action_key, location,
                                        std::string());
}

const Extension* ExtensionContextMenuModelTest::AddExtensionWithHostPermission(
    const std::string& name,
    const char* action_key,
    ManifestLocation location,
    const std::string& host_permission) {
  auto manifest = base::Value::Dict()
                      .Set("name", name)
                      .Set("version", "1")
                      .Set("manifest_version", 2);
  if (action_key)
    manifest.Set(action_key, base::Value::Dict());
  if (!host_permission.empty())
    manifest.Set("permissions", base::Value::List().Append(host_permission));
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(std::move(manifest))
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
    test_window_ = std::make_unique<TestBrowserWindow>();
    params.window = test_window_.get();
    browser_.reset(Browser::Create(params));
  }
  return browser_.get();
}

MenuManager* ExtensionContextMenuModelTest::CreateMenuManager() {
  return static_cast<MenuManager*>(
      MenuManagerFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(
                         &MenuManagerFactory::BuildServiceInstanceForTesting)));
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
ExtensionContextMenuModelTest::GetCommandState(
    const ExtensionContextMenuModel& menu,
    int command_id) const {
  bool is_present = menu.GetIndexOfCommandId(command_id).has_value();
  bool is_visible = menu.IsCommandIdVisible(command_id);

  // The command is absent if the menu entry is not present, or the entry is
  // present and not visible.
  if (!is_present || (is_present && !is_visible))
    return CommandState::kAbsent;

  // The command is disabled if the menu entry is present, visible and is not
  // enabled.
  bool is_enabled = menu.IsCommandIdEnabled(command_id);
  if (is_present && is_visible && !is_enabled)
    return CommandState::kDisabled;

  // Otherwise the command is enabled.
  return CommandState::kEnabled;
}

ExtensionContextMenuModelTest::CommandState
ExtensionContextMenuModelTest::GetPageAccessCommandState(
    const ExtensionContextMenuModel& menu,
    int command) const {
  // Check this method is called only for submenu page access commands.
  DCHECK(command == kOnClick || command == kOnSite || command == kOnAllSites ||
         command == kLearnMore || command == kPermissionsPage ||
         command == kPolicyInstalled);

  // Every page access command is absent if there is no page access submenu.
  if (!HasPageAccessSubmenu(menu))
    return CommandState::kAbsent;

  ui::MenuModel* submenu = menu.GetSubmenuModelAt(
      menu.GetIndexOfCommandId(ExtensionContextMenuModel::PAGE_ACCESS_SUBMENU)
          .value());
  DCHECK(submenu);

  ui::MenuModel** menu_to_search = &submenu;
  size_t index_unused = 0;
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
  return GetCommandState(menu,
                         ExtensionContextMenuModel::PAGE_ACCESS_SUBMENU) !=
         CommandState::kAbsent;
}

bool ExtensionContextMenuModelTest::HasCantAccessPageEntry(
    const ExtensionContextMenuModel& menu) const {
  CommandState cant_access_state =
      GetCommandState(menu, ExtensionContextMenuModel::PAGE_ACCESS_CANT_ACCESS);

  // The "Can't access this page" entry, if present, is always disabled.
  EXPECT_NE(cant_access_state, CommandState::kEnabled);
  return cant_access_state == CommandState::kDisabled;
}

void ExtensionContextMenuModelTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  display::Screen::SetScreenInstance(&test_screen_);
}

void ExtensionContextMenuModelTest::TearDown() {
  // Remove any tabs in the tab strip; else the test crashes.
  if (browser_) {
    while (!browser_->tab_strip_model()->empty())
      browser_->tab_strip_model()->DetachAndDeleteWebContentsAt(0);
  }

  display::Screen::SetScreenInstance(nullptr);
  ExtensionServiceTestBase::TearDown();
}

// Tests that applicable menu items are disabled when a ManagementPolicy
// prohibits them.
TEST_F(ExtensionContextMenuModelTest, RequiredInstallationsDisablesItems) {
  InitializeEmptyExtensionService();

  // Test that management policy can determine whether or not policy-installed
  // extensions can be installed/uninstalled.
  const Extension* extension =
      AddExtension("extension", manifest_keys::kPageAction,
                   ManifestLocation::kExternalPolicy);

  ExtensionSystem* system = ExtensionSystem::Get(profile());
  system->management_policy()->UnregisterAllProviders();

  {
    ExtensionContextMenuModel menu(extension, GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);

    // Uninstallation should be enabled when all policy provider were
    // unregistered.
    EXPECT_EQ(GetCommandState(menu, kUninstall), CommandState::kEnabled);
    EXPECT_EQ(GetCommandState(menu, kPolicyInstalled), CommandState::kAbsent);
  }

  TestManagementPolicyProvider policy_provider(
      TestManagementPolicyProvider::PROHIBIT_MODIFY_STATUS);
  system->management_policy()->RegisterProvider(&policy_provider);

  {
    ExtensionContextMenuModel menu(extension, GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);

    // If there's a policy provider that requires the extension stay enabled,
    // the uninstall item should be hidden and instead should display the
    // policy install disabled item.
    EXPECT_EQ(GetCommandState(menu, kUninstall), CommandState::kAbsent);
    EXPECT_EQ(GetCommandState(menu, kPolicyInstalled), CommandState::kDisabled);
  }

  // Don't leave |policy_provider| dangling.
  system->management_policy()->UnregisterProvider(&policy_provider);
}

// Tests the context menu for a component extension.
TEST_F(ExtensionContextMenuModelTest, ComponentExtensionContextMenu) {
  InitializeEmptyExtensionService();

  std::string name("component");
  base::Value::Dict manifest = base::Value::Dict()
                                   .Set("name", name)
                                   .Set("version", "1")
                                   .Set("manifest_version", 2)
                                   .Set("browser_action", base::Value::Dict());

  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder()
            .SetManifest(manifest.Clone())
            .SetID(crx_file::id_util::GenerateId("component"))
            .SetLocation(ManifestLocation::kComponent)
            .Build();
    service()->AddExtension(extension.get());

    ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);

    // A component extension's context menu should not include options for
    // managing extensions or removing it, and should only include an option for
    // the options page if the extension has one (which this one doesn't).
    EXPECT_EQ(GetCommandState(menu, ExtensionContextMenuModel::OPTIONS),
              CommandState::kAbsent);
    EXPECT_EQ(GetCommandState(menu, ExtensionContextMenuModel::UNINSTALL),
              CommandState::kAbsent);
    EXPECT_EQ(
        GetCommandState(menu, ExtensionContextMenuModel::MANAGE_EXTENSIONS),
        CommandState::kAbsent);

    // A component extension's context menu should not link to site settings.
    EXPECT_EQ(
        GetCommandState(menu, ExtensionContextMenuModel::VIEW_WEB_PERMISSIONS),
        CommandState::kAbsent);

    // The "name" option should be present, but not enabled for component
    // extensions.
    EXPECT_EQ(GetCommandState(menu, ExtensionContextMenuModel::HOME_PAGE),
              CommandState::kDisabled);
  }

  {
    // Check that a component extension with an options page does have the
    // options menu item, and it is enabled.
    manifest.Set("options_page", "options_page.html");
    scoped_refptr<const Extension> extension =
        ExtensionBuilder()
            .SetManifest(std::move(manifest))
            .SetID(crx_file::id_util::GenerateId("component_opts"))
            .SetLocation(ManifestLocation::kComponent)
            .Build();
    ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);
    service()->AddExtension(extension.get());
    EXPECT_TRUE(OptionsPageInfo::HasOptionsPage(extension.get()));
    EXPECT_EQ(GetCommandState(menu, ExtensionContextMenuModel::OPTIONS),
              CommandState::kEnabled);
  }
}

// Tests that the standard menu items (home page, uninstall, manage
// extensions, view web permissions) are always visible for any context menu
// source. NOTE: other menu items visibility is dependent on context, and
// behavior is checked in other tests.
TEST_F(ExtensionContextMenuModelTest,
       ExtensionContextMenuStandardItemsAlwaysVisible) {
  InitializeEmptyExtensionService();
  const Extension* extension = AddExtension(
      "extension", manifest_keys::kPageAction, ManifestLocation::kInternal);

  std::vector<ContextMenuSource> sources{ContextMenuSource::kToolbarAction,
                                         ContextMenuSource::kMenuItem};

  for (auto source : sources) {
    ExtensionContextMenuModel menu(extension, GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true, source);
    EXPECT_NE(GetCommandState(menu, ExtensionContextMenuModel::HOME_PAGE),
              CommandState::kAbsent);
    EXPECT_NE(GetCommandState(menu, ExtensionContextMenuModel::UNINSTALL),
              CommandState::kAbsent);
    EXPECT_NE(
        GetCommandState(menu, ExtensionContextMenuModel::MANAGE_EXTENSIONS),
        CommandState::kAbsent);
    EXPECT_NE(
        GetCommandState(menu, ExtensionContextMenuModel::VIEW_WEB_PERMISSIONS),
        CommandState::kAbsent);
  }
}

TEST_F(ExtensionContextMenuModelTest,
       ExtensionContextMenuToggleVisibilityEntryVisibility) {
  InitializeEmptyExtensionService();
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").Build();
  InitializeAndAddExtension(*extension);

  {
    // Verify the "toggle visibility" entry is absent if the context menu
    // source is a menu item.
    ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                   /*is_pinned=*/true, nullptr,
                                   /* can_show_icon_in_toolbar=*/true,
                                   ContextMenuSource::kMenuItem);
    EXPECT_FALSE(
        menu.GetIndexOfCommandId(ExtensionContextMenuModel::TOGGLE_VISIBILITY)
            .has_value());
    EXPECT_EQ(
        GetCommandState(menu, ExtensionContextMenuModel::TOGGLE_VISIBILITY),
        CommandState::kAbsent);
  }

  {
    // Verify the "toggle visibility" entry is absent if the context menu
    // source is a toolbar action and the icon cannot be shown in the toolbar.
    ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                   /*is_pinned=*/true, nullptr,
                                   /* can_show_icon_in_toolbar=*/false,
                                   ContextMenuSource::kToolbarAction);

    EXPECT_EQ(
        GetCommandState(menu, ExtensionContextMenuModel::TOGGLE_VISIBILITY),
        CommandState::kAbsent);
  }

  {
    // Verify the "toggle visibility" entry is enabled if and only if the
    // context menu source is a toolbar action and the icon can be shown in the
    // toolbar.
    ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                   /*is_pinned=*/true, nullptr,
                                   /* can_show_icon_in_toolbar=*/true,
                                   ContextMenuSource::kToolbarAction);
    EXPECT_EQ(
        GetCommandState(menu, ExtensionContextMenuModel::TOGGLE_VISIBILITY),
        CommandState::kEnabled);
  }
}

TEST_F(ExtensionContextMenuModelTest,
       ExtensionContextMenuOptionsEntryVisibility) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Extension")
          .SetManifestVersion(2)
          .SetID(crx_file::id_util::GenerateId("extension"))
          .Build();
  service()->AddExtension(extension.get());

  {
    // Verify the "options" entry is absent if the extension doesn't have
    // an options page.
    ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);
    EXPECT_EQ(GetCommandState(menu, ExtensionContextMenuModel::OPTIONS),
              CommandState::kAbsent);
  }

  scoped_refptr<const Extension> extension_with_options =
      ExtensionBuilder("Extension with options page")
          .SetManifestVersion(2)
          .SetID(crx_file::id_util::GenerateId("extension_with_options_page"))
          .SetManifestKey("options_page", "options_page.html")
          .Build();
  service()->AddExtension(extension_with_options.get());

  {
    // Verify the "options" entry is enabled if and only if the
    // extension has an options page.
    ExtensionContextMenuModel menu(extension_with_options.get(), GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);
    EXPECT_EQ(GetCommandState(menu, ExtensionContextMenuModel::OPTIONS),
              CommandState::kEnabled);
  }
}

// TODO(emiliapaz): Currently, the test scenarios always have "inspect popup"
// hidden since the context menu doesn't have a popup delegate and the developer
// mode pref is not set. Add a popup delegate and developer mode pref to
// properly test the "inspect popup" entry visibility.
TEST_F(ExtensionContextMenuModelTest,
       ExtensionContextMenuInspectPopupEntryVisibility) {
  InitializeEmptyExtensionService();
  {
    const Extension* page_action = AddExtension(
        "page_action", manifest_keys::kPageAction, ManifestLocation::kInternal);
    ASSERT_TRUE(page_action);
    ExtensionContextMenuModel menu(page_action, GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);
    EXPECT_EQ(GetCommandState(menu, ExtensionContextMenuModel::INSPECT_POPUP),
              CommandState::kAbsent);
  }

  {
    const Extension* browser_action =
        AddExtension("browser_action", manifest_keys::kBrowserAction,
                     ManifestLocation::kInternal);
    ExtensionContextMenuModel menu(browser_action, GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);
    EXPECT_EQ(GetCommandState(menu, ExtensionContextMenuModel::INSPECT_POPUP),
              CommandState::kAbsent);
  }

  {
    // An extension with no specified action has one synthesized. However,
    // there will never be a popup to inspect, so we shouldn't add a menu item.
    const Extension* no_action =
        AddExtension("no_action", nullptr, ManifestLocation::kInternal);
    ExtensionContextMenuModel menu(no_action, GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);
    EXPECT_EQ(GetCommandState(menu, ExtensionContextMenuModel::INSPECT_POPUP),
              CommandState::kAbsent);
  }
}

// Test that the "pin" and "unpin" menu items appear correctly in the extension
// context menu with toolbar action source.
TEST_F(ExtensionContextMenuModelTest, ExtensionContextMenuShowAndHide) {
  InitializeEmptyExtensionService();
  Browser* browser = GetBrowser();
  extension_action_test_util::CreateToolbarModelForProfile(profile());
  const Extension* page_action =
      AddExtension("page_action_extension", manifest_keys::kPageAction,
                   ManifestLocation::kInternal);
  const Extension* browser_action =
      AddExtension("browser_action_extension", manifest_keys::kBrowserAction,
                   ManifestLocation::kInternal);

  // For laziness.
  const ExtensionContextMenuModel::MenuEntries visibility_command =
      ExtensionContextMenuModel::TOGGLE_VISIBILITY;
  const std::u16string pin_string =
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_PIN_TO_TOOLBAR);
  const std::u16string unpin_string =
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_UNPIN_FROM_TOOLBAR);

  {
    // Even page actions should have a visibility option.
    ExtensionContextMenuModel menu(page_action, browser,
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);
    std::optional<size_t> index = menu.GetIndexOfCommandId(visibility_command);
    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(unpin_string, menu.GetLabelAt(index.value()));
  }

  {
    ExtensionContextMenuModel menu(browser_action, browser,
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);
    std::optional<size_t> index = menu.GetIndexOfCommandId(visibility_command);
    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(unpin_string, menu.GetLabelAt(index.value()));

    // Pin before unpinning.
    ToolbarActionsModel::Get(profile())->SetActionVisibility(
        browser_action->id(), true);
    menu.ExecuteCommand(visibility_command, 0);
  }

  {
    // If the action is unpinned, it should have the "Pin" string.
    ExtensionContextMenuModel menu(browser_action, browser,
                                   /*is_pinned,=*/false, nullptr, true,
                                   ContextMenuSource::kToolbarAction);
    std::optional<size_t> index = menu.GetIndexOfCommandId(visibility_command);
    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(pin_string, menu.GetLabelAt(index.value()));
  }
}

// Test that the "pin" and "unpin" menu items is disabled when the extension is
// force-pinned via ExtensionSettings.
TEST_F(ExtensionContextMenuModelTest, ExtensionContextMenuForcePinned) {
  InitializeEmptyExtensionService();
  Browser* browser = GetBrowser();
  extension_action_test_util::CreateToolbarModelForProfile(profile());
  const Extension* extension = AddExtension(
      "extension", manifest_keys::kBrowserAction, ManifestLocation::kInternal);
  const Extension* force_pinned_extension =
      AddExtension("force_pinned_extension", manifest_keys::kBrowserAction,
                   ManifestLocation::kInternal);

  std::string json = base::StringPrintf(
      R"({
        "%s": {
          "toolbar_pin": "force_pinned"
        }
      })",
      force_pinned_extension->id().c_str());
  std::optional<base::Value> parsed = base::JSONReader::Read(json);
  policy::PolicyMap map;
  map.Set("ExtensionSettings", policy::POLICY_LEVEL_MANDATORY,
          policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_PLATFORM,
          std::move(parsed), nullptr);
  policy_provider()->UpdateChromePolicy(map);

  // For laziness.
  const ExtensionContextMenuModel::MenuEntries visibility_command =
      ExtensionContextMenuModel::TOGGLE_VISIBILITY;
  const std::u16string unpin_string =
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_UNPIN_FROM_TOOLBAR);
  const std::u16string force_pinned_string =
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_PINNED_BY_ADMIN);

  {
    // Not force-pinned.
    ExtensionContextMenuModel menu(extension, browser,
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);
    std::optional<size_t> index = menu.GetIndexOfCommandId(visibility_command);
    ASSERT_TRUE(index.has_value());
    EXPECT_TRUE(menu.IsEnabledAt(index.value()));
    EXPECT_EQ(unpin_string, menu.GetLabelAt(index.value()));
  }

  {
    // Force-pinned.
    ExtensionContextMenuModel menu(force_pinned_extension, browser,
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);
    std::optional<size_t> index = menu.GetIndexOfCommandId(visibility_command);
    ASSERT_TRUE(index.has_value());
    EXPECT_FALSE(menu.IsEnabledAt(index.value()));
    EXPECT_EQ(force_pinned_string, menu.GetLabelAt(index.value()));
  }
}

TEST_F(ExtensionContextMenuModelTest, ExtensionContextUninstall) {
  InitializeEmptyExtensionService();

  const Extension* extension = AddExtension(
      "extension", manifest_keys::kBrowserAction, ManifestLocation::kInternal);
  const std::string extension_id = extension->id();
  ASSERT_TRUE(registry()->enabled_extensions().GetByID(extension_id));

  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);
  TestExtensionRegistryObserver uninstalled_observer(registry());
  {
    // Scope the menu so that it's destroyed during the uninstall process. This
    // reflects what normally happens (Chrome closes the menu when the uninstall
    // dialog shows up).
    ExtensionContextMenuModel menu(extension, GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);
    menu.ExecuteCommand(ExtensionContextMenuModel::UNINSTALL, 0);
  }
  uninstalled_observer.WaitForExtensionUninstalled();
  EXPECT_FALSE(registry()->GetExtensionById(extension_id,
                                            ExtensionRegistry::EVERYTHING));
}

TEST_F(ExtensionContextMenuModelTest, PageAccess_CustomizeByExtension_Submenu) {
  base::UserActionTester user_action_tester;
  constexpr char kOnClickAction[] =
      "Extensions.ContextMenu.Hosts.OnClickClicked";
  constexpr char kOnSiteAction[] = "Extensions.ContextMenu.Hosts.OnSiteClicked";
  constexpr char kOnAllSitesAction[] =
      "Extensions.ContextMenu.Hosts.OnAllSitesClicked";

  InitializeEmptyExtensionService();

  // Add an extension with all urls, and withhold permission.
  const Extension* extension =
      AddExtensionWithHostPermission("extension", manifest_keys::kBrowserAction,
                                     ManifestLocation::kInternal, "*://*/*");
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
  PermissionsManager* permissions_manager = PermissionsManager::Get(profile());
  const PermissionsData* permissions = extension->permissions_data();

  // Pretend the extension wants to run.
  int run_count = 0;
  auto increment_run_count_1 = base::BindOnce(&Increment, &run_count);
  action_runner->RequestScriptInjectionForTesting(
      extension, mojom::RunLocation::kDocumentIdle,
      std::move(increment_run_count_1));

  {
    ExtensionContextMenuModel menu(extension, GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);

    // Since we want to test the page access submenu, verify the site permission
    // is set to "customize by extension"  by default and the page access
    // submenu is visible.
    EXPECT_EQ(permissions_manager->GetUserSiteSetting(
                  url::Origin::Create(kActiveUrl)),
              PermissionsManager::UserSiteSetting::kCustomizeByExtension);
    EXPECT_EQ(GetCommandState(menu, kPageAccessSubmenu),
              CommandState::kEnabled);

    // Initial state: The extension should be in "run on click" mode.
    EXPECT_TRUE(menu.IsCommandIdChecked(kOnClick));
    EXPECT_FALSE(menu.IsCommandIdChecked(kOnSite));
    EXPECT_FALSE(menu.IsCommandIdChecked(kOnAllSites));

    // Initial state: The extension should have all permissions withheld, so
    // shouldn't be allowed to run on the active url or another arbitrary url,
    // and should have withheld permissions.

    EXPECT_FALSE(
        permissions_manager->HasGrantedHostPermission(*extension, kActiveUrl));
    EXPECT_FALSE(
        permissions_manager->HasGrantedHostPermission(*extension, kOtherUrl));
    EXPECT_FALSE(permissions->withheld_permissions().IsEmpty());

    EXPECT_EQ(0, user_action_tester.GetActionCount(kOnClickAction));
    EXPECT_EQ(0, user_action_tester.GetActionCount(kOnSiteAction));
    EXPECT_EQ(0, user_action_tester.GetActionCount(kOnAllSitesAction));

    // Change the mode to be "Run on site".
    menu.ExecuteCommand(kOnSite, 0);
    EXPECT_FALSE(menu.IsCommandIdChecked(kOnClick));
    EXPECT_TRUE(menu.IsCommandIdChecked(kOnSite));
    EXPECT_FALSE(menu.IsCommandIdChecked(kOnAllSites));
    EXPECT_EQ(0, user_action_tester.GetActionCount(kOnClickAction));
    EXPECT_EQ(1, user_action_tester.GetActionCount(kOnSiteAction));
    EXPECT_EQ(0, user_action_tester.GetActionCount(kOnAllSitesAction));

    // The extension should have access to the active url, but not to another
    // arbitrary url, and the extension should still have withheld permissions.
    EXPECT_TRUE(
        permissions_manager->HasGrantedHostPermission(*extension, kActiveUrl));
    EXPECT_FALSE(
        permissions_manager->HasGrantedHostPermission(*extension, kOtherUrl));
    EXPECT_FALSE(permissions->withheld_permissions().IsEmpty());

    // Since the extension has permission, it should have ran.
    EXPECT_EQ(1, run_count);
    EXPECT_FALSE(action_runner->WantsToRun(extension));
  }

  {
    // On another url, the mode should still be run on click. Context menu
    // usually closes on page navigations. There are some cases where
    // it doesn't, but we don't support dynamic updates. Thus, here we reopen
    // the context menu (there is another test that deals with page navigations
    // specifically).
    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(web_contents);
    web_contents_tester->NavigateAndCommit(kOtherUrl);

    ExtensionContextMenuModel menu(extension, GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);
    EXPECT_TRUE(menu.IsCommandIdChecked(kOnClick));
    EXPECT_FALSE(menu.IsCommandIdChecked(kOnSite));
    EXPECT_FALSE(menu.IsCommandIdChecked(kOnAllSites));
  }

  {
    // And returning to the first url should return the mode to run on site.
    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(web_contents);
    web_contents_tester->NavigateAndCommit(kActiveUrl);

    ExtensionContextMenuModel menu(extension, GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);
    EXPECT_FALSE(menu.IsCommandIdChecked(kOnClick));
    EXPECT_TRUE(menu.IsCommandIdChecked(kOnSite));
    EXPECT_FALSE(menu.IsCommandIdChecked(kOnAllSites));

    // Request another run.
    auto increment_run_count_2 = base::BindOnce(&Increment, &run_count);
    action_runner->RequestScriptInjectionForTesting(
        extension, mojom::RunLocation::kDocumentIdle,
        std::move(increment_run_count_2));

    // Change the mode to be "Run on all sites".
    menu.ExecuteCommand(kOnAllSites, 0);
    EXPECT_FALSE(menu.IsCommandIdChecked(kOnClick));
    EXPECT_FALSE(menu.IsCommandIdChecked(kOnSite));
    EXPECT_TRUE(menu.IsCommandIdChecked(kOnAllSites));
    EXPECT_EQ(0, user_action_tester.GetActionCount(kOnClickAction));
    EXPECT_EQ(1, user_action_tester.GetActionCount(kOnSiteAction));
    EXPECT_EQ(1, user_action_tester.GetActionCount(kOnAllSitesAction));

    // The extension should be able to run on any url, and shouldn't have any
    // withheld permissions.
    EXPECT_TRUE(
        permissions_manager->HasGrantedHostPermission(*extension, kActiveUrl));
    EXPECT_TRUE(
        permissions_manager->HasGrantedHostPermission(*extension, kOtherUrl));
    EXPECT_TRUE(permissions->withheld_permissions().IsEmpty());

    // It should have ran again.
    EXPECT_EQ(2, run_count);
    EXPECT_FALSE(action_runner->WantsToRun(extension));

    // On another url, the mode should also be run on all sites.
    web_contents_tester->NavigateAndCommit(kOtherUrl);
    EXPECT_FALSE(menu.IsCommandIdChecked(kOnClick));
    EXPECT_FALSE(menu.IsCommandIdChecked(kOnSite));
    EXPECT_TRUE(menu.IsCommandIdChecked(kOnAllSites));

    web_contents_tester->NavigateAndCommit(kActiveUrl);
    EXPECT_FALSE(menu.IsCommandIdChecked(kOnClick));
    EXPECT_FALSE(menu.IsCommandIdChecked(kOnSite));
    EXPECT_TRUE(menu.IsCommandIdChecked(kOnAllSites));

    auto increment_run_count_3 = base::BindOnce(&Increment, &run_count);
    action_runner->RequestScriptInjectionForTesting(
        extension, mojom::RunLocation::kDocumentIdle,
        std::move(increment_run_count_3));

    // Change extension to run "on click". Since we are revoking permissions, we
    // need to automatically accept the reload page bubble.
    action_runner->accept_bubble_for_testing(true);
    PermissionsManagerWaiter waiter(permissions_manager);
    menu.ExecuteCommand(kOnClick, 0);
    waiter.WaitForExtensionPermissionsUpdate();
    EXPECT_TRUE(menu.IsCommandIdChecked(kOnClick));
    EXPECT_FALSE(menu.IsCommandIdChecked(kOnSite));
    EXPECT_FALSE(menu.IsCommandIdChecked(kOnAllSites));
    EXPECT_EQ(1, user_action_tester.GetActionCount(kOnClickAction));
    EXPECT_EQ(1, user_action_tester.GetActionCount(kOnSiteAction));
    EXPECT_EQ(1, user_action_tester.GetActionCount(kOnAllSitesAction));

    // We should return to the initial state - no access.
    EXPECT_FALSE(
        permissions_manager->HasGrantedHostPermission(*extension, kActiveUrl));
    EXPECT_FALSE(
        permissions_manager->HasGrantedHostPermission(*extension, kOtherUrl));
    EXPECT_FALSE(permissions->withheld_permissions().IsEmpty());

    // And the extension shouldn't have ran.
    EXPECT_EQ(2, run_count);
    EXPECT_TRUE(action_runner->WantsToRun(extension));
  }

  {
    // Install an extension requesting a single host. The page access submenu
    // should still be present.
    const Extension* single_host_extension = AddExtensionWithHostPermission(
        "single_host_extension", manifest_keys::kBrowserAction,
        ManifestLocation::kInternal, "http://www.example.com/*");
    ExtensionContextMenuModel single_host_menu(
        single_host_extension, GetBrowser(), /*is_pinned=*/true, nullptr, true,
        ContextMenuSource::kToolbarAction);
    EXPECT_TRUE(
        single_host_menu
            .GetIndexOfCommandId(ExtensionContextMenuModel::PAGE_ACCESS_SUBMENU)
            .has_value());
  }
}

// Tests different permission patterns when the site setting is set to
// "customize by extension".
TEST_F(ExtensionContextMenuModelTest,
       PageAccess_CustomizeByExtension_PermissionPatterns) {
  InitializeEmptyExtensionService();

  struct {
    // The pattern requested by the extension.
    std::string requested_pattern;
    // The pattern that's granted to the extension, if any. This may be
    // significantly different than the requested pattern.
    std::optional<std::string> granted_pattern;
    // The current URL the context menu will be used on.
    GURL current_url;
    // The set of page access menu entries that should be present.
    std::set<MenuEntries> expected_entries;
    // The set of page access menu entries that should be enabled.
    std::set<MenuEntries> enabled_entries;
    // The selected page access menu entry.
    std::optional<MenuEntries> selected_entry;
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
       std::nullopt,
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
       std::nullopt,
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
      {"https://*.google.com/*", std::nullopt, GURL("https://example.com"), {}},
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

    // Site permission should be set to "customize by extension" by default.
    EXPECT_EQ(PermissionsManager::Get(profile())->GetUserSiteSetting(
                  url::Origin::Create(test_case.current_url)),
              PermissionsManager::UserSiteSetting::kCustomizeByExtension);

    ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);

    EXPECT_EQ(test_case.selected_entry.has_value(),
              !test_case.expected_entries.empty())
        << "If any entries are available, one should be selected.";

    if (test_case.expected_entries.empty()) {
      // If there are no expected entries (i.e., the extension can't run on the
      // page), there should be no submenu and instead there should be a
      // disabled label.
      EXPECT_TRUE(HasCantAccessPageEntry(menu));
      EXPECT_FALSE(HasPageAccessSubmenu(menu));
      continue;
    }

    // The learn more option should be visible whenever the page access submenu
    // is.
    EXPECT_EQ(CommandState::kEnabled,
              GetPageAccessCommandState(menu, kLearnMore));

    auto get_expected_state = [test_case](MenuEntries command) {
      if (!test_case.expected_entries.count(command))
        return CommandState::kAbsent;
      return test_case.enabled_entries.count(command) ? CommandState::kEnabled
                                                      : CommandState::kDisabled;
    };

    // Verify the submenu options are what we expect.
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
    std::u16string error;
    EXPECT_TRUE(service()->UninstallExtension(
        extension->id(), UNINSTALL_REASON_FOR_TESTING, &error));
    EXPECT_TRUE(error.empty()) << error;
    EXPECT_EQ(nullptr, registry()->GetInstalledExtension(extension->id()));
  }
}

// Test that changing to 'run on site' while already having an all_url like
// pattern actually removes the broad pattern to restrict to the site.
TEST_F(ExtensionContextMenuModelTest,
       PageAccess_CustomizeByExtension_OnSiteWithAllURLs) {
  InitializeEmptyExtensionService();

  // Add an extension with all urls, and withhold permissions.
  const Extension* extension =
      AddExtensionWithHostPermission("extension", manifest_keys::kBrowserAction,
                                     ManifestLocation::kInternal, "<all_urls>");
  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);

  // Grant the extension the all_urls pattern.
  URLPattern pattern(UserScript::ValidUserScriptSchemes(false), "<all_urls>");
  permissions_test_util::GrantRuntimePermissionsAndWaitForCompletion(
      profile(), *extension,
      PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                    URLPatternSet({pattern}), URLPatternSet()));
  PermissionsManager* permissions_manager = PermissionsManager::Get(profile());
  EXPECT_TRUE(permissions_manager->HasWithheldHostPermissions(*extension));

  const GURL kActiveUrl("http://www.example.com/");
  const GURL kOtherUrl("http://www.google.com/");

  // Navigate to a url that should have "customize by extension" site
  // permissions by default (which allows us to test the page access submenu).
  AddTab(kActiveUrl);
  EXPECT_EQ(PermissionsManager::Get(profile())->GetUserSiteSetting(
                url::Origin::Create(kActiveUrl)),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);

  // Verify the extension can run on all sites for the active url, and has
  // access to both urls.
  ExtensionContextMenuModel menu(extension, GetBrowser(),
                                 /*is_pinned=*/true, nullptr, true,
                                 ContextMenuSource::kToolbarAction);
  EXPECT_TRUE(HasPageAccessSubmenu(menu));
  EXPECT_FALSE(menu.IsCommandIdChecked(kOnClick));
  EXPECT_FALSE(menu.IsCommandIdChecked(kOnSite));
  EXPECT_TRUE(menu.IsCommandIdChecked(kOnAllSites));

  EXPECT_TRUE(
      permissions_manager->HasGrantedHostPermission(*extension, kActiveUrl));
  EXPECT_TRUE(
      permissions_manager->HasGrantedHostPermission(*extension, kOtherUrl));

  // Change mode to "Run on site".
  menu.ExecuteCommand(kOnSite, 0);
  EXPECT_FALSE(menu.IsCommandIdChecked(kOnClick));
  EXPECT_TRUE(menu.IsCommandIdChecked(kOnSite));
  EXPECT_FALSE(menu.IsCommandIdChecked(kOnAllSites));

  // The extension should have access to the active url, but not to another
  // arbitrary url.
  EXPECT_TRUE(
      permissions_manager->HasGrantedHostPermission(*extension, kActiveUrl));
  EXPECT_FALSE(
      permissions_manager->HasGrantedHostPermission(*extension, kOtherUrl));
}

// Test that changing to 'run on click' while having a broad pattern which
// doesn't actually overlap the current url, still actually removes that broad
// pattern and stops showing that the extension can run on all sites.
// TODO(tjudkins): This test is kind of bizarre in that it highlights a case
// where the submenu is displaying that extension can read data on all sites,
// when it can't actually read the site it is currently on. We should revisit
// what exactly the submenu should be conveying to the user about the current
// page and how that relates to the similar set of information on the Extension
// Settings page.
TEST_F(ExtensionContextMenuModelTest,
       PageAccess_Customize_ByExtension_OnClickWithBroadPattern) {
  InitializeEmptyExtensionService();

  // Add an extension with all urls, and withhold permissions.
  const Extension* extension =
      AddExtensionWithHostPermission("extension", manifest_keys::kBrowserAction,
                                     ManifestLocation::kInternal, "<all_urls>");
  ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(true);

  // Grant the extension a broad pattern which doesn't overlap the active url.
  URLPattern pattern(UserScript::ValidUserScriptSchemes(false), "*://*.org/*");
  permissions_test_util::GrantRuntimePermissionsAndWaitForCompletion(
      profile(), *extension,
      PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                    URLPatternSet({pattern}), URLPatternSet()));

  const GURL kActiveUrl("http://www.example.com/");
  const GURL kOrgUrl("http://chromium.org/");
  const GURL kOtherUrl("http://www.google.com/");

  // Also explicitly grant google.com.
  modifier.GrantHostPermission(kOtherUrl);
  PermissionsManager* permissions_manager = PermissionsManager::Get(profile());
  EXPECT_TRUE(permissions_manager->HasWithheldHostPermissions(*extension));

  // Navigate to a url that should have "customize by extension" site
  // permissions by default (which allows us to test the page access submenu).
  content::WebContents* web_contents = AddTab(kActiveUrl);
  EXPECT_EQ(
      permissions_manager->GetUserSiteSetting(url::Origin::Create(kActiveUrl)),
      PermissionsManager::UserSiteSetting::kCustomizeByExtension);

  // Verify the extension can run on all sites even though it
  // can't access the active url.
  ExtensionContextMenuModel menu(extension, GetBrowser(),
                                 /*is_pinned=*/true, nullptr, true,
                                 ContextMenuSource::kToolbarAction);
  EXPECT_TRUE(HasPageAccessSubmenu(menu));
  EXPECT_FALSE(menu.IsCommandIdChecked(kOnClick));
  EXPECT_FALSE(menu.IsCommandIdChecked(kOnSite));
  EXPECT_TRUE(menu.IsCommandIdChecked(kOnAllSites));

  EXPECT_FALSE(
      permissions_manager->HasGrantedHostPermission(*extension, kActiveUrl));
  EXPECT_TRUE(
      permissions_manager->HasGrantedHostPermission(*extension, kOrgUrl));
  EXPECT_TRUE(
      permissions_manager->HasGrantedHostPermission(*extension, kOtherUrl));

  // Change extension to run "on click". Since we are revoking permissions, we
  // need to automatically accept the reload page bubble.
  ExtensionActionRunner::GetForWebContents(web_contents)
      ->accept_bubble_for_testing(true);
  PermissionsManagerWaiter waiter(permissions_manager);
  menu.ExecuteCommand(kOnClick, 0);
  waiter.WaitForExtensionPermissionsUpdate();
  EXPECT_TRUE(menu.IsCommandIdChecked(kOnClick));
  EXPECT_FALSE(menu.IsCommandIdChecked(kOnSite));
  EXPECT_FALSE(menu.IsCommandIdChecked(kOnAllSites));

  // The broad org pattern should have been removed, but the explicit google
  // pattern should still remain.
  EXPECT_FALSE(
      permissions_manager->HasGrantedHostPermission(*extension, kActiveUrl));
  EXPECT_FALSE(
      permissions_manager->HasGrantedHostPermission(*extension, kOrgUrl));
  EXPECT_TRUE(
      permissions_manager->HasGrantedHostPermission(*extension, kOtherUrl));
}

TEST_F(ExtensionContextMenuModelTest,
       PageAccess_CustomizeByExtension_WithActiveTab) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").AddAPIPermission("activeTab").Build();
  InitializeAndAddExtension(*extension);

  // Navigate to a url that should have "customize by extension" site
  // permissions by default (which allows us to test the page access submenu).
  const GURL url("https://a.com");
  AddTab(url);
  EXPECT_EQ(PermissionsManager::Get(profile())->GetUserSiteSetting(
                url::Origin::Create(url)),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);

  ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                 /*is_pinned=*/true, nullptr, true,
                                 ContextMenuSource::kToolbarAction);
  EXPECT_TRUE(HasPageAccessSubmenu(menu));
  EXPECT_EQ(CommandState::kEnabled, GetPageAccessCommandState(menu, kOnClick));
  EXPECT_EQ(CommandState::kDisabled, GetPageAccessCommandState(menu, kOnSite));
  EXPECT_EQ(CommandState::kDisabled,
            GetPageAccessCommandState(menu, kOnAllSites));
  EXPECT_EQ(CommandState::kEnabled,
            GetPageAccessCommandState(menu, kLearnMore));
}

// Test that when there is a page navigation with the menu open the page access
// commands stay the same but we don't update site access when executing their
// actions.
TEST_F(ExtensionContextMenuModelTest,
       PageAccess_CustomizeByExtension_PageNavigation) {
  InitializeEmptyExtensionService();
  const GURL kOriginalUrl("http://www.example.com/");
  const GURL kNewUrl("http://www.chromium.org/");

  // Add an extension with all urls, and withhold permissions.
  const Extension* extension =
      AddExtensionWithHostPermission("extension", manifest_keys::kBrowserAction,
                                     ManifestLocation::kInternal, "<all_urls>");

  content::WebContents* web_contents = AddTab(kOriginalUrl);

  // Update kOriginalUrl to have "on site" site access. This will make all other
  // non-restricted urls to have "on click" site access.
  SitePermissionsHelper permissions(profile());
  permissions.UpdateSiteAccess(*extension, web_contents,
                               PermissionsManager::UserSiteAccess::kOnSite);

  PermissionsManager* permissions_manager = PermissionsManager::Get(profile());
  EXPECT_EQ(permissions_manager->GetUserSiteAccess(*extension, kOriginalUrl),
            PermissionsManager::UserSiteAccess::kOnSite);
  EXPECT_EQ(permissions_manager->GetUserSiteAccess(*extension, kNewUrl),
            PermissionsManager::UserSiteAccess::kOnClick);

  ExtensionContextMenuModel menu(extension, GetBrowser(),
                                 /*is_pinned=*/true, nullptr, true,
                                 ContextMenuSource::kToolbarAction);

  // All page access commands are enabled, and "on site" is selected.
  EXPECT_TRUE(menu.IsCommandIdEnabled(kOnClick));
  EXPECT_TRUE(menu.IsCommandIdEnabled(kOnSite));
  EXPECT_TRUE(menu.IsCommandIdEnabled(kOnAllSites));
  EXPECT_FALSE(menu.IsCommandIdChecked(kOnClick));
  EXPECT_TRUE(menu.IsCommandIdChecked(kOnSite));
  EXPECT_FALSE(menu.IsCommandIdChecked(kOnAllSites));

  // Navigate to another page with the menu open.
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents);
  web_contents_tester->NavigateAndCommit(kNewUrl);

  // Since we don't dynamically update the context menu, we are still showing
  // the content for kOriginalUrl. Thus, all the page access commands are still
  // enabled and "on site" is selected.
  EXPECT_TRUE(menu.IsCommandIdEnabled(kOnClick));
  EXPECT_TRUE(menu.IsCommandIdEnabled(kOnSite));
  EXPECT_TRUE(menu.IsCommandIdEnabled(kOnAllSites));
  EXPECT_FALSE(menu.IsCommandIdChecked(kOnClick));
  EXPECT_TRUE(menu.IsCommandIdChecked(kOnSite));
  EXPECT_FALSE(menu.IsCommandIdChecked(kOnAllSites));

  // Since we navigated to a different page, executing the command should not
  // update the site of either page.
  menu.ExecuteCommand(kOnAllSites, 0);
  EXPECT_EQ(permissions_manager->GetUserSiteAccess(*extension, kOriginalUrl),
            PermissionsManager::UserSiteAccess::kOnSite);
  EXPECT_EQ(permissions_manager->GetUserSiteAccess(*extension, kNewUrl),
            PermissionsManager::UserSiteAccess::kOnClick);
}

TEST_F(ExtensionContextMenuModelTest,
       TestTogglingAccessWithSpecificSitesWithUnrequestedUrl) {
  InitializeEmptyExtensionService();

  // Add an extension that wants access to a.com.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").AddHostPermission("*://a.com/*").Build();
  InitializeAndAddExtension(*extension);

  // Additionally, grant it the (unrequested) access to b.com.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  URLPattern b_com_pattern(Extension::kValidHostPermissionSchemes,
                           "*://b.com/*");
  PermissionSet b_com_permissions(APIPermissionSet(), ManifestPermissionSet(),
                                  URLPatternSet({b_com_pattern}),
                                  URLPatternSet());
  prefs->AddGrantedPermissions(extension->id(), b_com_permissions);

  PermissionsManager* permissions_manager = PermissionsManager::Get(profile());
  EXPECT_FALSE(permissions_manager->HasWithheldHostPermissions(*extension));

  const GURL a_com("https://a.com");
  content::WebContents* web_contents = AddTab(a_com);

  {
    ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);

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
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);
    EXPECT_FALSE(HasPageAccessSubmenu(menu));
    EXPECT_TRUE(HasCantAccessPageEntry(menu));
  }

  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);

  // However, if the extension has runtime-granted permissions to b.com, we
  // *should* display them in the menu.
  permissions_test_util::GrantRuntimePermissionsAndWaitForCompletion(
      profile(), *extension, b_com_permissions);

  {
    ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);
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

    // Set the extension to run "on click". Since we are revoking b.com
    // permissions, we need to automatically accept the reload page bubble.
    menu.ExecuteCommand(kOnClick, 0);
    ExtensionActionRunner::GetForWebContents(web_contents)
        ->accept_bubble_for_testing(true);
    PermissionsManagerWaiter waiter(permissions_manager);
    menu.ExecuteCommand(kOnClick, 0);
    waiter.WaitForExtensionPermissionsUpdate();
  }

  {
    PermissionsManager::ExtensionSiteAccess site_access =
        permissions_manager->GetSiteAccess(*extension, b_com);
    EXPECT_FALSE(site_access.has_site_access);
    EXPECT_FALSE(site_access.withheld_site_access);
  }

  ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                 /*is_pinned=*/true, nullptr, true,
                                 ContextMenuSource::kToolbarAction);
  // Somewhat strangely, this also removes the access controls, because we don't
  // show it for sites the extension doesn't want to run on.
  EXPECT_FALSE(HasPageAccessSubmenu(menu));
  EXPECT_TRUE(HasCantAccessPageEntry(menu));
}

TEST_F(ExtensionContextMenuModelTest,
       TestTogglingAccessWithSpecificSitesWithRequestedSites) {
  InitializeEmptyExtensionService();

  // Add an extension that wants access to a.com and b.com.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddHostPermissions({"*://a.com/*", "*://b.com/*"})
          .Build();
  InitializeAndAddExtension(*extension);

  PermissionsManager* permissions_manager = PermissionsManager::Get(profile());
  EXPECT_FALSE(permissions_manager->HasWithheldHostPermissions(*extension));

  const GURL a_com("https://a.com");
  content::WebContents* web_contents = AddTab(a_com);

  ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                 /*is_pinned=*/true, nullptr, true,
                                 ContextMenuSource::kToolbarAction);

  EXPECT_EQ(CommandState::kEnabled, GetPageAccessCommandState(menu, kOnClick));
  EXPECT_EQ(CommandState::kEnabled, GetPageAccessCommandState(menu, kOnSite));
  EXPECT_EQ(CommandState::kDisabled,
            GetPageAccessCommandState(menu, kOnAllSites));

  EXPECT_TRUE(menu.IsCommandIdChecked(kOnSite));
  EXPECT_FALSE(menu.IsCommandIdChecked(kOnClick));

  // Withhold access on a.com by setting the extension to run "on click". Since
  // we are revoking permissions, we need to automatically accept the reload
  // page bubble.
  ExtensionActionRunner::GetForWebContents(web_contents)
      ->accept_bubble_for_testing(true);
  PermissionsManagerWaiter waiter(permissions_manager);
  menu.ExecuteCommand(kOnClick, 0);
  waiter.WaitForExtensionPermissionsUpdate();

  // This, sadly, removes access for the extension on b.com as well. :( This
  // is because we revoke all host permissions when transitioning from "don't
  // withhold" to "do withhold".
  // TODO(devlin): We should fix that, so that toggling access on a.com doesn't
  // revoke access on b.com.
  const GURL b_com("https://b.com");
  PermissionsManager::ExtensionSiteAccess site_access =
      permissions_manager->GetSiteAccess(*extension, b_com);
  EXPECT_FALSE(site_access.has_site_access);
  EXPECT_TRUE(site_access.withheld_site_access);
}

TEST_F(ExtensionContextMenuModelTest, TestClickingPageAccessLearnMore) {
  base::UserActionTester user_action_tester;
  constexpr char kLearnMoreAction[] =
      "Extensions.ContextMenu.Hosts.LearnMoreClicked";
  InitializeEmptyExtensionService();

  // Add an extension that wants access to a.com.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").AddHostPermission("*://a.com/*").Build();
  InitializeAndAddExtension(*extension);

  PermissionsManager* permissions_manager = PermissionsManager::Get(profile());
  EXPECT_FALSE(permissions_manager->HasWithheldHostPermissions(*extension));

  const GURL a_com("https://a.com");
  AddTab(a_com);

  Browser* browser = GetBrowser();
  ExtensionContextMenuModel menu(extension.get(), browser,
                                 /*is_pinned=*/true, nullptr, true,
                                 ContextMenuSource::kToolbarAction);
  EXPECT_EQ(0, user_action_tester.GetActionCount(kLearnMoreAction));

  EXPECT_EQ(CommandState::kEnabled,
            GetPageAccessCommandState(menu, kLearnMore));
  menu.ExecuteCommand(kLearnMore, 0);

  EXPECT_EQ(2, browser->tab_strip_model()->count());
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(1, user_action_tester.GetActionCount(kLearnMoreAction));

  // Test web contents need a poke to commit.
  content::NavigationController& controller = web_contents->GetController();
  content::RenderFrameHostTester::CommitPendingLoad(&controller);

  EXPECT_EQ(
      GURL(extension_permissions_constants::kRuntimeHostPermissionsHelpURL),
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
                                     /*is_pinned=*/true, nullptr, true,
                                     ContextMenuSource::kToolbarAction);
    }
    tester.ExpectTotalCount(kHistogramName, 0);
  }

  {
    base::HistogramTester tester;
    {
      // The menu is constructed and shown, but no action is taken.
      ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                     /*is_pinned=*/true, nullptr, true,
                                     ContextMenuSource::kToolbarAction);
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
                                     /*is_pinned=*/true, nullptr, true,
                                     ContextMenuSource::kToolbarAction);
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
          .SetAction(ActionInfo::Type::kAction)
          .Build();
  InitializeAndAddExtension(*extension);

  MenuManager* const manager = CreateMenuManager();
  ASSERT_TRUE(manager);

  MenuBuilder builder(extension, GetBrowser(), manager);
  builder.AddContextItem(MenuItem::ACTION);
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

class ExtensionActionContextMenuModelTest
    : public ExtensionContextMenuModelTest,
      public testing::WithParamInterface<ActionInfo::Type> {};

TEST_P(ExtensionActionContextMenuModelTest,
       MenuItemShowsOnlyForAppropriateActionType) {
  const ActionInfo::Type action_type = GetParam();

  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      BuildExtensionWithActionType(action_type);
  service()->AddExtension(extension.get());

  MenuManager* const manager = CreateMenuManager();

  std::set<ActionInfo::Type> mismatched_types = {ActionInfo::Type::kPage,
                                                 ActionInfo::Type::kBrowser,
                                                 ActionInfo::Type::kAction};
  mismatched_types.erase(GetParam());

  // Currently, there are no associated context menu items.
  MenuBuilder builder(extension, GetBrowser(), manager);
  EXPECT_EQ(0, CountExtensionItems(*builder.BuildMenu()));

  for (ActionInfo::Type type : mismatched_types) {
    builder.AddContextItem(MenuItemContextForActionType(type));
    // Adding a menu item for an invalid type shouldn't result in a visible
    // menu item.
    EXPECT_EQ(0, CountExtensionItems(*builder.BuildMenu()));
  }

  builder.AddContextItem(MenuItemContextForActionType(action_type));
  EXPECT_EQ(1, CountExtensionItems(*builder.BuildMenu()));
}

TEST_P(ExtensionActionContextMenuModelTest, ActionMenuItemsAreLimited) {
  const ActionInfo::Type action_type = GetParam();
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      BuildExtensionWithActionType(action_type);
  service()->AddExtension(extension.get());

  MenuManager* const manager = CreateMenuManager();

  MenuBuilder builder(extension, GetBrowser(), manager);
  EXPECT_EQ(0, CountExtensionItems(*builder.BuildMenu()));

  const MenuItem::Context context_type =
      MenuItemContextForActionType(action_type);
  for (int i = 0; i < api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT + 1; ++i)
    builder.AddContextItem(context_type);

  // Even though LIMIT + 1 items were added, only LIMIT should be displayed.
  EXPECT_EQ(api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT,
            CountExtensionItems(*builder.BuildMenu()));
}

// Tests that top-level items adjust according to the visibility of others
// in the list.
TEST_P(ExtensionActionContextMenuModelTest,
       ActionItemsOverTheLimitAreShownIfSomeOthersAreHidden) {
  // This test uses hard-coded assumptions about the value of the top-level
  // limit in order to aid in readability. Assert that the value is expected.
  // Note: This can't be a static_assert() because the LIMIT is defined in a
  // different object file.
  ASSERT_EQ(6, api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT);
  const ActionInfo::Type action_type = GetParam();
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      BuildExtensionWithActionType(action_type);
  service()->AddExtension(extension.get());

  MenuManager* const manager = CreateMenuManager();

  MenuBuilder builder(extension, GetBrowser(), manager);
  EXPECT_EQ(0, CountExtensionItems(*builder.BuildMenu()));

  const MenuItem::Context context_type =
      MenuItemContextForActionType(action_type);
  constexpr int kNumItemsToAdd = 9;  // 3 over the limit.

  // Note: One-indexed; add exactly kNumItemsToAdd (9) items.
  for (int i = 1; i <= kNumItemsToAdd; ++i) {
    builder.AddContextItem(context_type);
    builder.SetItemTitle(i,
                         base::StringPrintf("%s%d", item_label().c_str(), i));
  }

  // We should cap the visible actions.
  EXPECT_EQ(api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT,
            CountExtensionItems(*builder.BuildMenu()));

  // By default, the additional action items have their visibility set to true.
  // Explicitly hide the eighth.
  builder.SetItemVisibility(8, false);

  {
    auto model = builder.BuildMenu();

    // The limit is still obeyed, so items 7 through 9 should not be visible.
    EXPECT_EQ(api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT,
              CountExtensionItems(*model));
    VerifyItems(*model, {"1", "2", "3", "4", "5", "6"});
  }

  // Hide the first two items.
  builder.SetItemVisibility(1, false);
  builder.SetItemVisibility(2, false);

  {
    auto model = builder.BuildMenu();
    // Hiding the first two items in the model should make visible the "extra"
    // items -- items 7 and 9. Note, item 8 was set to hidden, so it should not
    // show in the model.
    EXPECT_EQ(api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT,
              CountExtensionItems(*model));
    VerifyItems(*model, {"3", "4", "5", "6", "7", "9"});
  }

  // Unhide the eighth item.
  builder.SetItemVisibility(8, true);

  {
    auto model = builder.BuildMenu();
    // The ninth item should be replaced with the eighth.
    EXPECT_EQ(api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT,
              CountExtensionItems(*model));
    VerifyItems(*model, {"3", "4", "5", "6", "7", "8"});
  }

  // Unhide the first two items.
  builder.SetItemVisibility(1, true);
  builder.SetItemVisibility(2, true);

  {
    auto model = builder.BuildMenu();
    // Unhiding the first two items should put us back into the original state,
    // with only the first items displayed.
    EXPECT_EQ(api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT,
              CountExtensionItems(*model));
    VerifyItems(*model, {"1", "2", "3", "4", "5", "6"});
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExtensionActionContextMenuModelTest,
                         testing::Values(ActionInfo::Type::kPage,
                                         ActionInfo::Type::kBrowser,
                                         ActionInfo::Type::kAction));

class ExtensionContextMenuModelWithUserHostControlsTest
    : public ExtensionContextMenuModelTest,
      public testing::WithParamInterface<bool> {
 public:
  ExtensionContextMenuModelWithUserHostControlsTest() {
    feature_list_.InitWithFeatureState(
        extensions_features::kExtensionsMenuAccessControl, GetParam());
  }
  ~ExtensionContextMenuModelWithUserHostControlsTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ExtensionContextMenuModelWithUserHostControlsTest,
                         testing::Bool());

TEST_P(ExtensionContextMenuModelWithUserHostControlsTest,
       PageAccessItemsVisibilityBasedOnSiteSettings) {
  bool is_feature_enabled = GetParam();
  InitializeEmptyExtensionService();

  const Extension* extension =
      AddExtensionWithHostPermission("extension", manifest_keys::kBrowserAction,
                                     ManifestLocation::kInternal, "<all_urls>");

  // Add a tab to the browser.
  const GURL url("http://www.example.com/");
  AddTab(url);

  {
    // By default, the site permission is set to "customize by extension".
    // Verify "page access" submenu item and "permissions page" item are visible
    // and enabled.
    ExtensionContextMenuModel menu(extension, GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);
    EXPECT_EQ(GetCommandState(menu, kGrantAllExtensions),
              CommandState::kAbsent);
    EXPECT_EQ(GetCommandState(menu, kBlockAllExtensions),
              CommandState::kAbsent);
    EXPECT_EQ(GetCommandState(menu, kPageAccessSubmenu),
              CommandState::kEnabled);
    if (is_feature_enabled) {
      EXPECT_EQ(GetCommandState(menu, kPermissionsPage),
                CommandState::kEnabled);
      EXPECT_EQ(GetPageAccessCommandState(menu, kLearnMore),
                CommandState::kAbsent);
    } else {
      EXPECT_EQ(GetCommandState(menu, kPermissionsPage), CommandState::kAbsent);
      EXPECT_EQ(GetPageAccessCommandState(menu, kLearnMore),
                CommandState::kEnabled);
    }
  }

  // User site settings are only taken into account for site access computations
  // when the feature is enabled, even if they are added by the manager.
  // Therefore, the context menu should not take into account user site settings
  // when the feature is disabled.
  auto* manager = PermissionsManager::Get(profile());

  {
    // Add site as a user restricted site.
    PermissionsManagerWaiter manager_waiter(manager);
    manager->AddUserRestrictedSite(url::Origin::Create(url));
    manager_waiter.WaitForUserPermissionsSettingsChange();

    ExtensionContextMenuModel menu(extension, GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);

    if (is_feature_enabled) {
      // Verify "block all extensions" item is visible and disabled, and
      // "permissions page" is enabled in the context menu. "learn more" is
      // never visible.
      EXPECT_EQ(GetCommandState(menu, kGrantAllExtensions),
                CommandState::kAbsent);
      EXPECT_EQ(GetCommandState(menu, kBlockAllExtensions),
                CommandState::kDisabled);
      EXPECT_EQ(GetCommandState(menu, kPageAccessSubmenu),
                CommandState::kAbsent);
      EXPECT_EQ(GetCommandState(menu, kPermissionsPage),
                CommandState::kEnabled);
      EXPECT_EQ(GetCommandState(menu, kLearnMore), CommandState::kAbsent);
      EXPECT_EQ(GetPageAccessCommandState(menu, kLearnMore),
                CommandState::kAbsent);
    } else {
      // Even though we added a site as a user restricted site, the site
      // permission behaves as "customize by extension". Verify page access
      // submenu is visible and enabled, the "learn more" item is in in the
      // submenu and the "permissions page" item is nowhere visible.
      EXPECT_EQ(GetCommandState(menu, kGrantAllExtensions),
                CommandState::kAbsent);
      EXPECT_EQ(GetCommandState(menu, kBlockAllExtensions),
                CommandState::kAbsent);
      EXPECT_EQ(GetCommandState(menu, kPageAccessSubmenu),
                CommandState::kEnabled);
      EXPECT_EQ(GetCommandState(menu, kLearnMore), CommandState::kAbsent);
      EXPECT_EQ(GetPageAccessCommandState(menu, kLearnMore),
                CommandState::kEnabled);
      EXPECT_EQ(GetCommandState(menu, kPermissionsPage), CommandState::kAbsent);
      EXPECT_EQ(GetPageAccessCommandState(menu, kPermissionsPage),
                CommandState::kAbsent);
    }
  }
}

TEST_P(ExtensionContextMenuModelWithUserHostControlsTest,
       PageAccessItemsVisibility_PolicyInstalled) {
  bool is_feature_enabled = GetParam();
  InitializeEmptyExtensionService();

  const Extension* enterprise_extension = AddExtensionWithHostPermission(
      "extension", manifest_keys::kBrowserAction,
      ManifestLocation::kExternalPolicy, "<all_urls>");

  // Add a tab to the browser.
  const GURL url("http://www.example.com/");
  AddTab(url);

  ExtensionContextMenuModel menu(enterprise_extension, GetBrowser(),
                                 /*is_pinned=*/true, nullptr, true,
                                 ContextMenuSource::kToolbarAction);

  // By default, user can customize site access by extension and the 'grant all
  // extensions' and 'block all extensions' are not visible.
  EXPECT_EQ(GetCommandState(menu, kGrantAllExtensions), CommandState::kAbsent);
  EXPECT_EQ(GetCommandState(menu, kBlockAllExtensions), CommandState::kAbsent);

  if (is_feature_enabled) {
    // Page access submenu is enabled and has all its items disabled, since
    // the policy installed extension has site access but user cannot change it.
    EXPECT_EQ(GetCommandState(menu, kPageAccessSubmenu),
              CommandState::kEnabled);
    EXPECT_EQ(GetPageAccessCommandState(menu, kOnClick),
              CommandState::kDisabled);
    EXPECT_EQ(GetPageAccessCommandState(menu, kOnSite),
              CommandState::kDisabled);
    EXPECT_EQ(GetPageAccessCommandState(menu, kOnAllSites),
              CommandState::kDisabled);
    EXPECT_FALSE(menu.IsCommandIdChecked(kOnClick));
    EXPECT_FALSE(menu.IsCommandIdChecked(kOnSite));
    EXPECT_TRUE(menu.IsCommandIdChecked(kOnAllSites));
    // Policy item is in the page access submenu.
    EXPECT_EQ(GetPageAccessCommandState(menu, kPolicyInstalled),
              CommandState::kDisabled);
  } else {
    // Page access submenu is hidden since user cannot change the site access of
    // the policy installed extension.
    EXPECT_EQ(GetCommandState(menu, kPageAccessSubmenu), CommandState::kAbsent);
    EXPECT_EQ(GetPageAccessCommandState(menu, kOnClick), CommandState::kAbsent);
    EXPECT_EQ(GetPageAccessCommandState(menu, kOnSite), CommandState::kAbsent);
    EXPECT_EQ(GetPageAccessCommandState(menu, kOnAllSites),
              CommandState::kAbsent);
    // Policy item is in the context menu.
    EXPECT_EQ(GetCommandState(menu, kPolicyInstalled), CommandState::kDisabled);
  }
}

TEST_P(ExtensionContextMenuModelWithUserHostControlsTest,
       PolicyInstalledEntryVisibilityBasedOnSiteSettings) {
  bool is_feature_enabled = GetParam();
  InitializeEmptyExtensionService();

  const Extension* extension =
      AddExtensionWithHostPermission("Extension", manifest_keys::kBrowserAction,
                                     ManifestLocation::kInternal, "<all_urls>");
  const Extension* enterprise_extension =
      AddExtension("Enterprise extension", manifest_keys::kBrowserAction,
                   ManifestLocation::kExternalPolicy);
  const Extension* enterprise_extension_host_permissions =
      AddExtensionWithHostPermission(
          "Enterprise extension requesting host permissions",
          manifest_keys::kBrowserAction, ManifestLocation::kExternalPolicy,
          "<all_urls>");

  // Add a tab to the browser.
  const GURL url("http://www.example.com/");
  AddTab(url);

  {
    // Non-enterprise extension has no policy entry.
    ExtensionContextMenuModel menu(extension, GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);
    EXPECT_EQ(GetPageAccessCommandState(menu, kPolicyInstalled),
              CommandState::kAbsent);
    EXPECT_EQ(GetCommandState(menu, kPolicyInstalled), CommandState::kAbsent);
  }

  {
    // By default, the site permission is set to "customize by extension".
    // Verify "policy installed" entry is visible and disabled in the main menu
    // when extension doesn't request host permissions.
    ExtensionContextMenuModel menu(enterprise_extension, GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);
    EXPECT_EQ(GetPageAccessCommandState(menu, kPolicyInstalled),
              CommandState::kAbsent);
    EXPECT_EQ(GetCommandState(menu, kPolicyInstalled), CommandState::kDisabled);

    // Verify "policy installed" entry is visible and disabled when extension
    // requests host permissions. Entry is in the page access submenu when the
    // feature is enabled, otherwise is on the main menu.
    ExtensionContextMenuModel menu_host_permissions(
        enterprise_extension_host_permissions, GetBrowser(),
        /*is_pinned=*/true, nullptr, true, ContextMenuSource::kToolbarAction);
    if (is_feature_enabled) {
      EXPECT_EQ(
          GetPageAccessCommandState(menu_host_permissions, kPolicyInstalled),
          CommandState::kDisabled);
      EXPECT_EQ(GetCommandState(menu_host_permissions, kPolicyInstalled),
                CommandState::kAbsent);
    } else {
      EXPECT_EQ(GetPageAccessCommandState(menu, kPolicyInstalled),
                CommandState::kAbsent);
      EXPECT_EQ(GetCommandState(menu, kPolicyInstalled),
                CommandState::kDisabled);
    }
  }

  // User site settings are only taken into account for site access computations
  // when the feature is enabled, even if they are added by the manager.
  // Therefore, the context menu should not take into account user site settings
  // when the feature is disabled.
  auto* manager = PermissionsManager::Get(profile());

  {
    // Add site as a user restricted site.
    PermissionsManagerWaiter manager_waiter(manager);
    manager->AddUserRestrictedSite(url::Origin::Create(url));
    manager_waiter.WaitForUserPermissionsSettingsChange();

    // Verify "policy installed" entry is visible and disabled in the main menu
    // when extension doesn't request host permissions.
    ExtensionContextMenuModel menu(enterprise_extension, GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);
    EXPECT_EQ(GetPageAccessCommandState(menu, kPolicyInstalled),
              CommandState::kAbsent);
    EXPECT_EQ(GetCommandState(menu, kPolicyInstalled), CommandState::kDisabled);

    // Verify "policy installed" entry is visible and disabled when extension
    // requests host permissions. Entry is in the page access submenu when the
    // feature is enabled, otherwise is on the main menu.
    ExtensionContextMenuModel menu_host_permissions(
        enterprise_extension_host_permissions, GetBrowser(),
        /*is_pinned=*/true, nullptr, true, ContextMenuSource::kToolbarAction);

    if (is_feature_enabled) {
      EXPECT_EQ(
          GetPageAccessCommandState(menu_host_permissions, kPolicyInstalled),
          CommandState::kDisabled);
      EXPECT_EQ(GetCommandState(menu_host_permissions, kPolicyInstalled),
                CommandState::kAbsent);
    } else {
      EXPECT_EQ(
          GetPageAccessCommandState(menu_host_permissions, kPolicyInstalled),
          CommandState::kAbsent);
      EXPECT_EQ(GetCommandState(menu_host_permissions, kPolicyInstalled),
                CommandState::kDisabled);
    }
  }
}

// Tests that the uninstall entry is only visible for extensions that can be
// uninstalled (e.g non-enterprise extensions).
TEST_P(ExtensionContextMenuModelWithUserHostControlsTest,
       UninstallEntryVisibility) {
  InitializeEmptyExtensionService();

  const Extension* extension = AddExtension(
      "Extension", manifest_keys::kBrowserAction, ManifestLocation::kInternal);
  const Extension* enterprise_extension =
      AddExtension("Enterprise extension", manifest_keys::kBrowserAction,
                   ManifestLocation::kExternalPolicy);

  const GURL url("http://www.example.com/");
  AddTab(url);

  {
    // Verify non-enterprise extension has uninstall entry.
    ExtensionContextMenuModel menu(extension, GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);
    EXPECT_EQ(GetCommandState(menu, kUninstall), CommandState::kEnabled);
  }

  {
    // Verify enterprise extension does not have uninstall entry.
    ExtensionContextMenuModel menu(enterprise_extension, GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);
    EXPECT_EQ(GetCommandState(menu, kUninstall), CommandState::kAbsent);
  }
}

// Test clicking on the "permissions page" item opens the correct link.
TEST_P(ExtensionContextMenuModelWithUserHostControlsTest,
       TestClickingPageAccessPermissionsPage) {
  base::UserActionTester user_action_tester;
  constexpr char kPermissionsPageAction[] =
      "Extensions.ContextMenu.Hosts.PermissionsPageClicked";
  InitializeEmptyExtensionService();

  // Add an extension that wants access to a.com.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").AddHostPermission("*://a.com/*").Build();
  InitializeAndAddExtension(*extension);

  EXPECT_FALSE(PermissionsManager::Get(profile())->HasWithheldHostPermissions(
      *extension));

  AddTab(GURL("https://a.com"));

  Browser* browser = GetBrowser();
  ExtensionContextMenuModel menu(extension.get(), browser,
                                 /*is_pinned=*/true, nullptr, true,
                                 ContextMenuSource::kToolbarAction);
  EXPECT_EQ(user_action_tester.GetActionCount(kPermissionsPageAction), 0);

  // "permissions page" button is not visible when the enhanced host controls
  // feature is disabled.
  if (!GetParam()) {
    EXPECT_EQ(GetPageAccessCommandState(menu, kPermissionsPage),
              CommandState::kAbsent);
    // There's nothing more we need to test in this case.
    return;
  }

  EXPECT_EQ(GetCommandState(menu, kPermissionsPage), CommandState::kEnabled);
  menu.ExecuteCommand(kPermissionsPage, 0);

  EXPECT_EQ(browser->tab_strip_model()->count(), 2);
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(user_action_tester.GetActionCount(kPermissionsPageAction), 1);

  // Test web contents need a poke to commit.
  content::NavigationController& controller = web_contents->GetController();
  content::RenderFrameHostTester::CommitPendingLoad(&controller);

  EXPECT_EQ(
      web_contents->GetLastCommittedURL(),
      GURL(extension_permissions_constants::kExtensionsSitePermissionsURL));
}

class ExtensionContextMenuModelWithUserHostControlsAndPermittedSitesTest
    : public ExtensionContextMenuModelWithUserHostControlsTest {
 public:
  ExtensionContextMenuModelWithUserHostControlsAndPermittedSitesTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionsMenuAccessControlWithPermittedSites);
  }
  ~ExtensionContextMenuModelWithUserHostControlsAndPermittedSitesTest()
      override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ExtensionContextMenuModelWithUserHostControlsAndPermittedSitesTest,
    testing::Bool());

TEST_P(ExtensionContextMenuModelWithUserHostControlsAndPermittedSitesTest,
       PageAccessItemsVisibilityBasedOnSiteSettings) {
  bool is_feature_enabled = GetParam();
  InitializeEmptyExtensionService();

  const Extension* extension =
      AddExtensionWithHostPermission("extension", manifest_keys::kBrowserAction,
                                     ManifestLocation::kInternal, "<all_urls>");

  // Add a tab to the browser.
  const GURL url("http://www.example.com/");
  AddTab(url);

  {
    // By default, the site permission is set to "customize by extension".
    // Verify "page access" submenu item is visible and enabled.
    ExtensionContextMenuModel menu(extension, GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);
    EXPECT_EQ(GetCommandState(menu, kGrantAllExtensions),
              CommandState::kAbsent);
    EXPECT_EQ(GetCommandState(menu, kBlockAllExtensions),
              CommandState::kAbsent);
    EXPECT_EQ(GetCommandState(menu, kPageAccessSubmenu),
              CommandState::kEnabled);
    if (is_feature_enabled) {
      EXPECT_EQ(GetCommandState(menu, kPermissionsPage),
                CommandState::kEnabled);
      EXPECT_EQ(GetPageAccessCommandState(menu, kLearnMore),
                CommandState::kAbsent);
    } else {
      EXPECT_EQ(GetCommandState(menu, kPermissionsPage), CommandState::kAbsent);
      EXPECT_EQ(GetPageAccessCommandState(menu, kLearnMore),
                CommandState::kEnabled);
    }
  }

  // User site settings are only taken into account for site access computations
  // when the kExtensionsMenuAccessControl feature is enabled, even if they are
  // added by the manager. Therefore, the context menu should not take into
  // account user site settings when the feature is disabled.
  auto* manager = PermissionsManager::Get(profile());

  {
    // Add site as a user permitted site.
    PermissionsManagerWaiter manager_waiter(manager);
    manager->AddUserPermittedSite(url::Origin::Create(url));
    manager_waiter.WaitForUserPermissionsSettingsChange();

    ExtensionContextMenuModel menu(extension, GetBrowser(),
                                   /*is_pinned=*/true, nullptr, true,
                                   ContextMenuSource::kToolbarAction);

    if (is_feature_enabled) {
      // Verify "grant all extensions" item is visible and disabled, and
      // "permissions page" is enabled in the context menu. "learn more" is
      // never visible.
      EXPECT_EQ(GetCommandState(menu, kGrantAllExtensions),
                CommandState::kDisabled);
      EXPECT_EQ(GetCommandState(menu, kBlockAllExtensions),
                CommandState::kAbsent);
      EXPECT_EQ(GetCommandState(menu, kPageAccessSubmenu),
                CommandState::kAbsent);
      EXPECT_EQ(GetCommandState(menu, kPermissionsPage),
                CommandState::kEnabled);
      EXPECT_EQ(GetCommandState(menu, kLearnMore), CommandState::kAbsent);
      EXPECT_EQ(GetPageAccessCommandState(menu, kLearnMore),
                CommandState::kAbsent);
    } else {
      // Even though we added a site as a user permitted site, the site
      // permission behaves as "customize by extension". Verify page access
      // submenu is visible and enabled, the "learn more" item is in in the
      // submenu and the "permissions page" item is nowhere visible.
      EXPECT_EQ(GetCommandState(menu, kGrantAllExtensions),
                CommandState::kAbsent);
      EXPECT_EQ(GetCommandState(menu, kBlockAllExtensions),
                CommandState::kAbsent);
      EXPECT_EQ(GetCommandState(menu, kPageAccessSubmenu),
                CommandState::kEnabled);
      EXPECT_EQ(GetCommandState(menu, kLearnMore), CommandState::kAbsent);
      EXPECT_EQ(GetPageAccessCommandState(menu, kLearnMore),
                CommandState::kEnabled);
      EXPECT_EQ(GetCommandState(menu, kPermissionsPage), CommandState::kAbsent);
      EXPECT_EQ(GetPageAccessCommandState(menu, kPermissionsPage),
                CommandState::kAbsent);
    }
  }
}

}  // namespace extensions
