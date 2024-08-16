// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/app_list_test_util.h"
#include "chrome/browser/ash/app_list/app_service/app_service_app_item.h"
#include "chrome/browser/ash/app_list/app_service/app_service_context_menu.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "chrome/browser/ash/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ash/app_list/test/fake_app_list_model_updater.h"
#include "chrome/browser/ash/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/browser/ash/arc/icon_decode_request.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "chromeos/ash/components/standalone_browser/migrator_util.h"
#include "components/app_constants/constants.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/user_manager/scoped_user_manager.h"
#include "extensions/common/manifest_constants.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/test/test_screen.h"

namespace app_list {

namespace {

class FakeAppContextMenuDelegate : public AppContextMenuDelegate {
 public:
  FakeAppContextMenuDelegate() = default;
  FakeAppContextMenuDelegate(const FakeAppContextMenuDelegate&) = delete;
  FakeAppContextMenuDelegate& operator=(const FakeAppContextMenuDelegate&) =
      delete;
  ~FakeAppContextMenuDelegate() override = default;

  // AppContextMenuDelegate overrides:
  void ExecuteLaunchCommand(int event_flags) override {}
};

class FakeAppListControllerDelegate
    : public ::test::TestAppListControllerDelegate {
 public:
  FakeAppListControllerDelegate() = default;
  FakeAppListControllerDelegate(const FakeAppListControllerDelegate&) = delete;
  FakeAppListControllerDelegate& operator=(
      const FakeAppListControllerDelegate&) = delete;
  ~FakeAppListControllerDelegate() override = default;

  void SetAppPinnable(const std::string& app_id, Pinnable type) {
    pinnable_apps_[app_id] = type;
  }

  void SetAppOpen(const std::string& app_id, bool open) {
    if (open)
      open_apps_.insert(app_id);
    else
      open_apps_.erase(app_id);
  }

  bool IsAppOpen(const std::string& app_id) const override {
    return open_apps_.count(app_id) != 0;
  }

  // test::TestAppListControllerDelegate overrides:
  Pinnable GetPinnable(const std::string& app_id) override {
    std::map<std::string, Pinnable>::const_iterator it;
    it = pinnable_apps_.find(app_id);
    if (it == pinnable_apps_.end())
      return NO_PIN;
    return it->second;
  }

 private:
  std::map<std::string, Pinnable> pinnable_apps_;
  std::unordered_set<std::string> open_apps_;
};

class FakeAppServiceAppItem : public AppServiceAppItem {
 public:
  FakeAppServiceAppItem(Profile* profile,
                        AppListModelUpdater* model_updater,
                        const AppListSyncableService::SyncItem* sync_item,
                        const apps::AppUpdate& app_update)
      : AppServiceAppItem(profile, model_updater, sync_item, app_update) {}
  FakeAppServiceAppItem(const FakeAppServiceAppItem&) = delete;
  FakeAppServiceAppItem& operator=(const FakeAppServiceAppItem&) = delete;
  ~FakeAppServiceAppItem() override = default;

  // AppContextMenuDelegate overrides:
  void ExecuteLaunchCommand(int event_flags) override {
    AppServiceAppItem::ExecuteLaunchCommand(event_flags);

    if (!quit_callback_.is_null())
      std::move(quit_callback_).Run();
  }

  void WaitForLaunch() {
    base::RunLoop run_loop;
    quit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  base::OnceClosure quit_callback_;
};

std::unique_ptr<KeyedService> MenuManagerFactory(
    content::BrowserContext* context) {
  return extensions::MenuManagerFactory::BuildServiceInstanceForTesting(
      context);
}

std::unique_ptr<FakeAppServiceAppItem> GetAppListItem(
    Profile* profile,
    const std::string& app_id) {
  std::unique_ptr<FakeAppServiceAppItem> item;
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(app_id, [profile, &item](const apps::AppUpdate& update) {
        item = std::make_unique<FakeAppServiceAppItem>(
            profile, /*model_updater=*/nullptr, /*sync_item=*/nullptr, update);

        // Because model updater is null, set position manually.
        item->SetChromePosition(item->CalculateDefaultPositionForTest());
      });
  return item;
}

std::unique_ptr<ui::SimpleMenuModel> GetContextMenuModel(
    ChromeAppListItem* item) {
  base::RunLoop run_loop;
  std::unique_ptr<ui::SimpleMenuModel> menu;
  item->GetContextMenuModel(
      ash::AppListItemContext::kNone,
      base::BindLambdaForTesting(
          [&](std::unique_ptr<ui::SimpleMenuModel> created_menu) {
            menu = std::move(created_menu);
            run_loop.Quit();
          }));
  run_loop.Run();
  return menu;
}

std::unique_ptr<ui::SimpleMenuModel> GetMenuModel(
    AppContextMenu* context_menu) {
  base::RunLoop run_loop;
  std::unique_ptr<ui::SimpleMenuModel> menu;
  context_menu->GetMenuModel(base::BindLambdaForTesting(
      [&](std::unique_ptr<ui::SimpleMenuModel> created_menu) {
        menu = std::move(created_menu);
        run_loop.Quit();
      }));
  run_loop.Run();
  return menu;
}

}  // namespace

class AppContextMenuTest : public AppListTestBase {
 public:
  AppContextMenuTest() { display::Screen::SetScreenInstance(&test_screen_); }
  AppContextMenuTest(const AppContextMenuTest&) = delete;
  AppContextMenuTest& operator=(const AppContextMenuTest&) = delete;
  ~AppContextMenuTest() override {
    display::Screen::SetScreenInstance(nullptr);
  }

  void SetUp() override {
    AppListTestBase::SetUp();
    extensions::MenuManagerFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&MenuManagerFactory));
    controller_ = std::make_unique<FakeAppListControllerDelegate>();
    menu_delegate_ = std::make_unique<FakeAppContextMenuDelegate>();
    model_updater_ = std::make_unique<FakeAppListModelUpdater>(
        /*profile=*/nullptr, /*reorder_delegate=*/nullptr);
    ChromeAppListItem::OverrideAppListControllerDelegateForTesting(
        controller());
  }

  void TearDown() override {
    // Let any in-flight tasks finish, e.g. clear the background thread icon
    // decode, otherwise the test might flake (crbug.com/1115763).
    base::RunLoop().RunUntilIdle();
    menu_delegate_.reset();
    controller_.reset();
    menu_manager_.reset();
  }

 protected:
  struct MenuState {
    // Defines separator.
    MenuState() : command_id(-1), is_enabled(true), is_checked(false) {}

    // Defines enabled unchecked command.
    explicit MenuState(int command_id)
        : command_id(command_id), is_enabled(true), is_checked(false) {}

    MenuState(int command_id, bool enabled, bool checked)
        : command_id(command_id), is_enabled(enabled), is_checked(checked) {}

    int command_id;
    bool is_enabled;
    bool is_checked;
  };

  void ValidateItemState(const ui::MenuModel* menu_model,
                         size_t index,
                         const MenuState& state) {
    EXPECT_EQ(state.command_id, menu_model->GetCommandIdAt(index));
    if (state.command_id == -1)
      return;  // Don't check separator.
    EXPECT_EQ(state.is_enabled, menu_model->IsEnabledAt(index));
    EXPECT_EQ(state.is_checked, menu_model->IsItemCheckedAt(index));
  }

  void ValidateMenuState(const ui::MenuModel* menu_model,
                         const std::vector<MenuState>& states) {
    ASSERT_NE(nullptr, menu_model);
    size_t state_index = 0;
    for (size_t i = 0; i < menu_model->GetItemCount(); ++i) {
      ASSERT_LT(state_index, states.size());
      ValidateItemState(menu_model, i, states[state_index++]);
    }
    EXPECT_EQ(state_index, states.size());
  }

  FakeAppListControllerDelegate* controller() { return controller_.get(); }

  FakeAppContextMenuDelegate* menu_delegate() { return menu_delegate_.get(); }

  Profile* profile() { return profile_.get(); }

  void AddToStates(const AppServiceContextMenu& menu,
                   MenuState state,
                   std::vector<MenuState>* states) {
    // If the command is not enabled do not add it to states.
    if (!menu.IsCommandIdEnabled(state.command_id))
      return;

    states->push_back(state);
  }

  scoped_refptr<extensions::Extension> MakeApp(const std::string& app_id,
                                               bool platform_app) {
    base::FilePath path;
    base::PathService::Get(chrome::DIR_TEST_DATA, &path);
    path = path.AppendASCII("extensions").AppendASCII("manifest_tests");
    base::FilePath manifest_path =
        (platform_app) ? path.AppendASCII("init_valid_platform_app.json")
                       : path.AppendASCII("hosted_app_absolute_options.json");

    JSONFileValueDeserializer deserializer(manifest_path);
    base::Value manifest = base::Value::FromUniquePtrValue(
        deserializer.Deserialize(nullptr, nullptr));

    DCHECK(manifest.is_dict());
    std::string error;
    return extensions::Extension::Create(
        path.DirName(), extensions::mojom::ManifestLocation::kInternal,
        manifest.GetDict(), extensions::Extension::NO_FLAGS, app_id, &error);
  }

  void TestExtensionApp(const std::string& app_id,
                        bool platform_app,
                        AppListControllerDelegate::Pinnable pinnable,
                        extensions::LaunchType launch_type) {
    app_service_test_.SetUp(profile());

    scoped_refptr<extensions::Extension> store = MakeApp(app_id, platform_app);
    service_->AddExtension(store.get());
    service_->EnableExtension(app_id);

    controller_ = std::make_unique<FakeAppListControllerDelegate>();
    controller_->SetAppPinnable(app_id, pinnable);
    controller_->SetExtensionLaunchType(profile(), app_id, launch_type);

    AppServiceContextMenu menu(menu_delegate(), profile(), app_id, controller(),
                               ash::AppListItemContext::kNone);
    std::unique_ptr<ui::MenuModel> menu_model = GetMenuModel(&menu);
    ASSERT_NE(nullptr, menu_model);

    std::vector<MenuState> states;
    if (!platform_app)
      AddToStates(menu, MenuState(ash::LAUNCH_NEW), &states);

    if (pinnable != AppListControllerDelegate::NO_PIN) {
      AddToStates(
          menu,
          MenuState(ash::TOGGLE_PIN,
                    pinnable != AppListControllerDelegate::PIN_FIXED, false),
          &states);
    }
    if (!platform_app)
      AddToStates(menu, MenuState(ash::OPTIONS), &states);
    AddToStates(menu, MenuState(ash::UNINSTALL), &states);
    AddToStates(menu, MenuState(ash::SHOW_APP_INFO), &states);

    ValidateMenuState(menu_model.get(), states);
  }

  scoped_refptr<extensions::Extension> MakeChromeApp() {
    std::string err;
    base::Value::Dict value;
    value.Set("name", "Chrome App");
    value.Set("version", "0.0");
    value.SetByDottedPath("app.launch.web_url", "http://google.com");
    scoped_refptr<extensions::Extension> app = extensions::Extension::Create(
        base::FilePath(), extensions::mojom::ManifestLocation::kInternal, value,
        extensions::Extension::WAS_INSTALLED_BY_DEFAULT,
        app_constants::kChromeAppId, &err);
    EXPECT_EQ(err, "");
    return app;
  }

  void TestChromeApp() {
    app_service_test_.SetUp(profile());

    scoped_refptr<extensions::Extension> store = MakeChromeApp();
    service_->AddExtension(store.get());

    controller_ = std::make_unique<FakeAppListControllerDelegate>();
    AppServiceContextMenu menu(menu_delegate(), profile(),
                               app_constants::kChromeAppId, controller(),
                               ash::AppListItemContext::kNone);
    std::unique_ptr<ui::MenuModel> menu_model = GetMenuModel(&menu);
    ASSERT_NE(nullptr, menu_model);

    std::vector<MenuState> states;
    AddToStates(menu, MenuState(ash::APP_CONTEXT_MENU_NEW_WINDOW), &states);
    if (!profile()->IsOffTheRecord())
      AddToStates(menu, MenuState(ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW),
                  &states);
    AddToStates(menu, MenuState(ash::SHOW_APP_INFO), &states);
    ValidateMenuState(menu_model.get(), states);
  }

  apps::AppServiceTest& app_service_test() { return app_service_test_; }

 private:
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  display::test::TestScreen test_screen_;
  std::unique_ptr<KeyedService> menu_manager_;
  std::unique_ptr<FakeAppListControllerDelegate> controller_;
  std::unique_ptr<FakeAppContextMenuDelegate> menu_delegate_;
  std::unique_ptr<FakeAppListModelUpdater> model_updater_;
  apps::AppServiceTest app_service_test_;
};

TEST_F(AppContextMenuTest, ExtensionApp) {
  for (extensions::LaunchType launch_type = extensions::LAUNCH_TYPE_FIRST;
       launch_type < extensions::NUM_LAUNCH_TYPES;
       launch_type = static_cast<extensions::LaunchType>(launch_type + 1)) {
    AppListControllerDelegate::Pinnable pinnable;
    for (pinnable = AppListControllerDelegate::NO_PIN;
         pinnable <= AppListControllerDelegate::PIN_FIXED;
         pinnable =
             static_cast<AppListControllerDelegate::Pinnable>(pinnable + 1)) {
      for (bool is_platform_app : {false, true}) {
        TestExtensionApp(AppListTestBase::kHostedAppId, is_platform_app,
                         pinnable, launch_type);
        TestExtensionApp(AppListTestBase::kPackagedApp1Id, is_platform_app,
                         pinnable, launch_type);
        TestExtensionApp(AppListTestBase::kPackagedApp2Id, is_platform_app,
                         pinnable, launch_type);
      }
    }
  }
}

TEST_F(AppContextMenuTest, ChromeApp) {
  TestChromeApp();
}

TEST_F(AppContextMenuTest, ChromeAppInRecentAppsList) {
  app_service_test().SetUp(profile());

  scoped_refptr<extensions::Extension> app = MakeChromeApp();
  service_->AddExtension(app.get());

  // Simulate a context menu in the recent apps row.
  AppServiceContextMenu menu(menu_delegate(), profile(),
                             app_constants::kChromeAppId, controller(),
                             ash::AppListItemContext::kRecentApps);
  std::unique_ptr<ui::MenuModel> menu_model = GetMenuModel(&menu);
  ASSERT_NE(nullptr, menu_model);

  // The usual chrome menu items appear.
  std::vector<MenuState> states;
  AddToStates(menu, MenuState(ash::APP_CONTEXT_MENU_NEW_WINDOW), &states);
  AddToStates(menu, MenuState(ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW),
              &states);
  AddToStates(menu, MenuState(ash::SHOW_APP_INFO), &states);
  ValidateMenuState(menu_model.get(), states);
}

TEST_F(AppContextMenuTest, NonExistingExtensionApp) {
  AppServiceContextMenu menu(menu_delegate(), profile(),
                             "some_non_existing_extension_app", controller(),
                             ash::AppListItemContext::kNone);
  std::unique_ptr<ui::MenuModel> menu_model = GetMenuModel(&menu);
  EXPECT_EQ(nullptr, menu_model);
}

TEST_F(AppContextMenuTest, ArcMenu) {
  app_service_test().SetUp(profile());
  ArcAppTest arc_test;
  arc_test.SetUp(profile());

  const auto& app_info = arc_test.fake_apps()[1];
  const std::string app_id = ArcAppTest::GetAppId(*app_info);
  controller()->SetAppPinnable(app_id, AppListControllerDelegate::PIN_EDITABLE);

  arc_test.app_instance()->SendRefreshAppList(arc_test.fake_apps());

  std::unique_ptr<FakeAppServiceAppItem> item =
      GetAppListItem(profile(), app_id);

  std::unique_ptr<ui::MenuModel> menu = GetContextMenuModel(item.get());
  ASSERT_NE(nullptr, menu);

  // Separators are not added to touchable app context menus. For touchable app
  // context menus, arc app has double separator, three more app shortcuts
  // provided by arc::FakeAppInstance and two separators between shortcuts.
  const size_t expected_items = 10;

  ASSERT_EQ(expected_items, menu->GetItemCount());
  size_t index = 0;
  ValidateItemState(menu.get(), index++, MenuState(ash::LAUNCH_NEW));
  ValidateItemState(menu.get(), index++, MenuState(ash::TOGGLE_PIN));
  ValidateItemState(menu.get(), index++, MenuState(ash::UNINSTALL));
  ValidateItemState(menu.get(), index++, MenuState(ash::SHOW_APP_INFO));

  // Test activate request.
  EXPECT_EQ(0u, arc_test.app_instance()->launch_requests().size());

  menu->ActivatedAt(0);

  // Wait for the async menu item to be executed to launch the app.
  item->WaitForLaunch();

  const std::vector<std::unique_ptr<arc::FakeAppInstance::Request>>&
      launch_requests = arc_test.app_instance()->launch_requests();
  ASSERT_EQ(1u, launch_requests.size());
  EXPECT_TRUE(launch_requests[0]->IsForApp(*app_info));

  controller()->SetAppOpen(app_id, true);
  arc_test.app_instance()->SendTaskCreated(1, *app_info, std::string());

  // It is not expected that menu model is unchanged on GetContextMenuModel.
  // ARC app menu requires model to be recalculated.
  menu = GetContextMenuModel(item.get());

  // Separators are not added to touchable app context menus except for arc app
  // shortcuts, which have double separator, three more app shortcuts provided
  // by arc::FakeAppInstance and two separators between shortcuts.
  const size_t expected_items_app_open = 9;
  ASSERT_EQ(expected_items_app_open, menu->GetItemCount());
  index = 0;
  ValidateItemState(menu.get(), index++, MenuState(ash::TOGGLE_PIN));
  ValidateItemState(menu.get(), index++, MenuState(ash::UNINSTALL));
  ValidateItemState(menu.get(), index++, MenuState(ash::SHOW_APP_INFO));

  // Test that arc app shortcuts provided by arc::FakeAppInstance have a
  // separator between each app shortcut.
  EXPECT_EQ(ui::DOUBLE_SEPARATOR, menu->GetSeparatorTypeAt(index++));
  for (int shortcut_index = 0; index < menu->GetItemCount(); ++index) {
    EXPECT_EQ(base::StringPrintf("ShortLabel %d", shortcut_index++),
              base::UTF16ToUTF8(menu->GetLabelAt(index++)));
    if (index < menu->GetItemCount())
      EXPECT_EQ(ui::PADDED_SEPARATOR, menu->GetSeparatorTypeAt(index));
  }

  // Test launching app shortcut item.
  EXPECT_EQ(0, arc_test.app_instance()->launch_app_shortcut_item_count());
  menu->ActivatedAt(menu->GetItemCount() - 1);
  EXPECT_EQ(1, arc_test.app_instance()->launch_app_shortcut_item_count());

  // This makes all apps non-ready.
  controller()->SetAppOpen(app_id, false);
  arc_test.app_instance()->SendTaskDestroyed(1);
  arc::ConnectionObserver<arc::mojom::AppInstance>* connection_observer =
      arc_test.arc_app_list_prefs();
  connection_observer->OnConnectionClosed();

  menu = GetContextMenuModel(item.get());

  // Separators and disabled options are not added to touchable app context
  // menus. For touchable app context menus, arc app has double separator,
  // three more app shortcuts provided by arc::FakeAppInstance and two
  // separators between shortcuts.
  const size_t expected_items_reopen = 8;
  ASSERT_EQ(expected_items_reopen, menu->GetItemCount());
  index = 0;
  ValidateItemState(menu.get(), index++, MenuState(ash::LAUNCH_NEW));
  ValidateItemState(menu.get(), index++, MenuState(ash::TOGGLE_PIN));

  // Test that arc app shortcuts provided by arc::FakeAppInstance have a
  // separator between each app shortcut.
  EXPECT_EQ(ui::DOUBLE_SEPARATOR, menu->GetSeparatorTypeAt(index++));
  for (int shortcut_index = 0; index < menu->GetItemCount(); ++index) {
    EXPECT_EQ(base::StringPrintf("ShortLabel %d", shortcut_index++),
              base::UTF16ToUTF8(menu->GetLabelAt(index++)));
    if (index < menu->GetItemCount())
      EXPECT_EQ(ui::PADDED_SEPARATOR, menu->GetSeparatorTypeAt(index));
  }

  // Uninstall all apps.
  arc_test.app_instance()->SendRefreshAppList(
      std::vector<arc::mojom::AppInfoPtr>());
  controller()->SetAppOpen(app_id, false);

  // No app available case.
  menu = GetContextMenuModel(item.get());
  EXPECT_EQ(nullptr, menu);
}

TEST_F(AppContextMenuTest, ArcMenuShortcut) {
  app_service_test().SetUp(profile());
  ArcAppTest arc_test;
  arc_test.SetUp(profile());

  const arc::mojom::ShortcutInfo& shortcut_info = arc_test.fake_shortcuts()[0];
  const std::string app_id = ArcAppTest::GetAppId(shortcut_info);
  controller()->SetAppPinnable(app_id, AppListControllerDelegate::PIN_EDITABLE);

  arc_test.app_instance()->SendInstallShortcuts(arc_test.fake_shortcuts());

  std::unique_ptr<AppServiceAppItem> item = GetAppListItem(profile(), app_id);

  std::unique_ptr<ui::MenuModel> menu = GetContextMenuModel(item.get());
  ASSERT_NE(nullptr, menu);
  // Separators are not added to touchable app context menus. For touchable app
  // context menus, arc app has double separator, three more app shortcuts
  // provided by arc::FakeAppInstance and two separators between shortcuts.
  const size_t expected_items = 10;
  size_t index = 0;
  ASSERT_EQ(expected_items, menu->GetItemCount());
  ValidateItemState(menu.get(), index++, MenuState(ash::LAUNCH_NEW));
  ValidateItemState(menu.get(), index++, MenuState(ash::TOGGLE_PIN));
  ValidateItemState(menu.get(), index++, MenuState(ash::UNINSTALL));
  ValidateItemState(menu.get(), index++, MenuState(ash::SHOW_APP_INFO));
  // Test that arc app shortcuts provided by arc::FakeAppInstance have a
  // separator between each app shortcut.
  EXPECT_EQ(ui::DOUBLE_SEPARATOR, menu->GetSeparatorTypeAt(index++));
  for (int shortcut_index = 0; index < menu->GetItemCount(); ++index) {
    EXPECT_EQ(base::StringPrintf("ShortLabel %d", shortcut_index++),
              base::UTF16ToUTF8(menu->GetLabelAt(index++)));
    if (index < menu->GetItemCount())
      EXPECT_EQ(ui::PADDED_SEPARATOR, menu->GetSeparatorTypeAt(index));
  }

  // This makes all apps non-ready. Shortcut is still uninstall-able.
  arc::ConnectionObserver<arc::mojom::AppInstance>* connection_observer =
      arc_test.arc_app_list_prefs();
  connection_observer->OnConnectionClosed();

  menu = GetContextMenuModel(item.get());
  // Separators and disabled options are not added to touchable app context
  // menus. For touchable app context menus, arc app has double separator,
  // three more app shortcuts provided by arc::FakeAppInstance and two
  // separators between shortcuts.
  const size_t expected_items_non_ready = 9;
  ASSERT_EQ(expected_items_non_ready, menu->GetItemCount());
  index = 0;
  ValidateItemState(menu.get(), index++, MenuState(ash::LAUNCH_NEW));
  ValidateItemState(menu.get(), index++, MenuState(ash::TOGGLE_PIN));
  ValidateItemState(menu.get(), index++, MenuState(ash::UNINSTALL));

  // Test that arc app shortcuts provided by arc::FakeAppInstance have a
  // separator between each app shortcut.
  EXPECT_EQ(ui::DOUBLE_SEPARATOR, menu->GetSeparatorTypeAt(index++));
  for (int shortcut_index = 0; index < menu->GetItemCount(); ++index) {
    EXPECT_EQ(base::StringPrintf("ShortLabel %d", shortcut_index++),
              base::UTF16ToUTF8(menu->GetLabelAt(index++)));
    if (index < menu->GetItemCount())
      EXPECT_EQ(ui::PADDED_SEPARATOR, menu->GetSeparatorTypeAt(index));
  }
}

TEST_F(AppContextMenuTest, ArcMenuStickyItem) {
  app_service_test().SetUp(profile());
  ArcAppTest arc_test;
  arc_test.SetUp(profile());

  arc_test.app_instance()->SendRefreshAppList(arc_test.fake_apps());

  {
    // Verify menu of store
    const auto& store_info = arc_test.fake_apps()[0];
    const std::string store_id = ArcAppTest::GetAppId(*store_info);
    controller()->SetAppPinnable(store_id,
                                 AppListControllerDelegate::PIN_EDITABLE);
    std::unique_ptr<AppServiceAppItem> item =
        GetAppListItem(profile(), store_id);
    std::unique_ptr<ui::MenuModel> menu = GetContextMenuModel(item.get());
    ASSERT_NE(nullptr, menu);

    // Separators are not added to touchable app context menus. For touchable
    // app context menus, arc app has double separator, three more app shortcuts
    // provided by arc::FakeAppInstance and two separators between shortcuts.
    size_t expected_items = 9;
    ASSERT_EQ(expected_items, menu->GetItemCount());
    size_t index = 0;
    ValidateItemState(menu.get(), index++, MenuState(ash::LAUNCH_NEW));
    ValidateItemState(menu.get(), index++, MenuState(ash::TOGGLE_PIN));
    ValidateItemState(menu.get(), index++, MenuState(ash::SHOW_APP_INFO));

    // Test that arc app shortcuts provided by arc::FakeAppInstance have a
    // separator between each app shortcut.
    EXPECT_EQ(ui::DOUBLE_SEPARATOR, menu->GetSeparatorTypeAt(index++));
    for (int shortcut_index = 0; index < menu->GetItemCount(); ++index) {
      EXPECT_EQ(base::StringPrintf("ShortLabel %d", shortcut_index++),
                base::UTF16ToUTF8(menu->GetLabelAt(index++)));
      if (index < menu->GetItemCount())
        EXPECT_EQ(ui::PADDED_SEPARATOR, menu->GetSeparatorTypeAt(index));
    }
  }
}

// In suspended state app does not have launch item.
TEST_F(AppContextMenuTest, ArcMenuSuspendedItem) {
  app_service_test().SetUp(profile());
  ArcAppTest arc_test;
  arc_test.SetUp(profile());

  std::vector<arc::mojom::AppInfoPtr> apps;
  apps.emplace_back(arc_test.fake_apps()[0]->Clone())->suspended = true;
  arc_test.app_instance()->SendRefreshAppList(apps);

  const std::string app_id = ArcAppTest::GetAppId(*apps[0]);
  controller()->SetAppPinnable(app_id, AppListControllerDelegate::PIN_EDITABLE);
  std::unique_ptr<AppServiceAppItem> item = GetAppListItem(profile(), app_id);
  std::unique_ptr<ui::MenuModel> menu = GetContextMenuModel(item.get());
  ASSERT_NE(nullptr, menu);

  // Separators are not added to touchable app context menus. For touchable
  // app context menus, arc app has double separator, three more app shortcuts
  // provided by arc::FakeAppInstance and two separators between shortcuts.
  size_t expected_items = 8;
  ASSERT_EQ(expected_items, menu->GetItemCount());
  size_t index = 0;
  ValidateItemState(menu.get(), index++, MenuState(ash::TOGGLE_PIN));
  ValidateItemState(menu.get(), index++, MenuState(ash::SHOW_APP_INFO));

  // Test that arc app shortcuts provided by arc::FakeAppInstance have a
  // separator between each app shortcut.
  EXPECT_EQ(ui::DOUBLE_SEPARATOR, menu->GetSeparatorTypeAt(index++));
  for (int shortcut_index = 0; index < menu->GetItemCount(); ++index) {
    EXPECT_EQ(base::StringPrintf("ShortLabel %d", shortcut_index++),
              base::UTF16ToUTF8(menu->GetLabelAt(index++)));
    if (index < menu->GetItemCount())
      EXPECT_EQ(ui::PADDED_SEPARATOR, menu->GetSeparatorTypeAt(index));
  }
}

TEST_F(AppContextMenuTest, CommandIdsMatchEnumsForHistograms) {
  // Tests that CommandId enums are not changed as the values are used in
  // histograms.
  EXPECT_EQ(9, ash::NOTIFICATION_CONTAINER);
  EXPECT_EQ(100, ash::LAUNCH_NEW);
  EXPECT_EQ(101, ash::TOGGLE_PIN);
  EXPECT_EQ(102, ash::SHOW_APP_INFO);
  EXPECT_EQ(103, ash::OPTIONS);
  EXPECT_EQ(104, ash::UNINSTALL);
  EXPECT_EQ(105, ash::REMOVE_FROM_FOLDER);
  EXPECT_EQ(106, ash::APP_CONTEXT_MENU_NEW_WINDOW);
  EXPECT_EQ(107, ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW);
  EXPECT_EQ(108, ash::INSTALL);
  EXPECT_EQ(200, ash::USE_LAUNCH_TYPE_COMMAND_START);
  EXPECT_EQ(200, ash::DEPRECATED_USE_LAUNCH_TYPE_PINNED);
  EXPECT_EQ(201, ash::USE_LAUNCH_TYPE_REGULAR);
  EXPECT_EQ(202, ash::DEPRECATED_USE_LAUNCH_TYPE_FULLSCREEN);
  EXPECT_EQ(203, ash::USE_LAUNCH_TYPE_WINDOW);
  EXPECT_EQ(204, ash::USE_LAUNCH_TYPE_TABBED_WINDOW);
}

// Tests that internal app's context menu is correct.
TEST_F(AppContextMenuTest, InternalAppMenu) {
  for (const auto& internal_app : GetInternalAppList(profile())) {
    controller()->SetAppPinnable(internal_app.app_id,
                                 AppListControllerDelegate::PIN_EDITABLE);

    std::unique_ptr<AppServiceAppItem> item =
        GetAppListItem(profile(), internal_app.app_id);
    std::unique_ptr<ui::MenuModel> menu = GetContextMenuModel(item.get());
    ASSERT_NE(nullptr, menu);
    EXPECT_EQ(1u, menu->GetItemCount());
    ValidateItemState(menu.get(), 0, MenuState(ash::TOGGLE_PIN));
  }
}

// Lacros has its own test suite because the feature needs to be enabled before
// SetUp().
class AppContextMenuLacrosTest : public AppContextMenuTest {
 public:
  AppContextMenuLacrosTest() {
    feature_list_.InitWithFeatures(
        ash::standalone_browser::GetFeatureRefs(),
        {ash::features::kEnforceAshExtensionKeeplist});
    ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForTest(
        true);
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kEnableLacrosForTesting);
  }
  AppContextMenuLacrosTest(const AppContextMenuLacrosTest&) = delete;
  AppContextMenuLacrosTest& operator=(const AppContextMenuLacrosTest&) = delete;
  ~AppContextMenuLacrosTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());

    // Login a user. The "email" must match the TestingProfile's
    // GetProfileUserName() so that profile() will be the primary profile.
    const AccountId account_id = AccountId::FromUserEmail("testing_profile");
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);

    // Creates profile().
    AppContextMenuTest::SetUp();

    ASSERT_TRUE(ash::ProfileHelper::Get()->IsPrimaryProfile(profile()));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
};

TEST_F(AppContextMenuLacrosTest, LacrosApp) {
  app_service_test().SetUp(profile());

  // Create the context menu.
  AppServiceContextMenu menu(menu_delegate(), profile(),
                             app_constants::kLacrosAppId, controller(),
                             ash::AppListItemContext::kNone);
  std::unique_ptr<ui::MenuModel> menu_model = GetMenuModel(&menu);
  ASSERT_NE(menu_model, nullptr);

  // Verify expected menu items.
  // It should have, Open new window, Open incognito window, and app info.
  EXPECT_EQ(menu_model->GetItemCount(), 3u);
  std::vector<MenuState> states;
  AddToStates(menu, MenuState(ash::APP_CONTEXT_MENU_NEW_WINDOW), &states);
  AddToStates(menu, MenuState(ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW),
              &states);
  AddToStates(menu, MenuState(ash::SHOW_APP_INFO), &states);
  ValidateMenuState(menu_model.get(), states);
}

}  // namespace app_list
