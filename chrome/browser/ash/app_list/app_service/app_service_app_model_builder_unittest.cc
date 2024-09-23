// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_service/app_service_app_model_builder.h"

#include <memory>
#include <set>
#include <string>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/icon_standardizer.h"
#include "chrome/browser/ash/app_list/app_list_test_util.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "chrome/browser/ash/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ash/app_list/md_icon_normalizer.h"
#include "chrome/browser/ash/app_list/test/fake_app_list_model_updater.h"
#include "chrome/browser/ash/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/testing/features.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_test_helper.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_test_helper.h"
#include "chrome/browser/extensions/chrome_app_icon.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/image_loader.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/test/test_screen.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_unittest_util.h"

using crostini::CrostiniTestHelper;
using extensions::AppSorting;
using extensions::ExtensionSystem;
using plugin_vm::PluginVmTestHelper;
using ::testing::_;
using ::testing::Matcher;

namespace app_list {

namespace {

const size_t kDefaultAppCount = 3u;

constexpr char kRootFolderName[] = "Linux apps";
constexpr char kDummyApp1Name[] = "dummy1";
constexpr char kDummyApp2Id[] = "dummy2";
constexpr char kDummyApp2Name[] = "dummy2";
constexpr char kAppNewName[] = "new name";
constexpr char kBananaAppId[] = "banana";
constexpr char kBananaAppName[] = "banana app name";

// Convenience matcher for some important fields of the chrome app.
MATCHER_P3(IsChromeApp, id, name, folder_id, "") {
  Matcher<std::string> id_m(id);
  Matcher<std::string> name_m(name);
  Matcher<std::string> folder_id_m(folder_id);
  return id_m.Matches(arg->id()) && name_m.Matches(arg->name()) &&
         folder_id_m.Matches(arg->folder_id());
}

// Matches a chrome app item if its persistence field is set to true.
MATCHER(IsSystemFolder, "") {
  return arg->is_system_folder();
}

// Get a set of all apps in |model|.
std::vector<std::string> GetModelContent(AppListModelUpdater* model_updater) {
  return base::ToVector(model_updater->GetItems(), &ChromeAppListItem::name);
}

scoped_refptr<extensions::Extension> MakeApp(const std::string& name,
                                             const std::string& version,
                                             const std::string& url,
                                             const std::string& id) {
  std::string err;
  base::Value::Dict value;
  value.Set("name", name);
  value.Set("version", version);
  value.SetByDottedPath("app.launch.web_url", url);
  scoped_refptr<extensions::Extension> app = extensions::Extension::Create(
      base::FilePath(), extensions::mojom::ManifestLocation::kInternal, value,
      extensions::Extension::WAS_INSTALLED_BY_DEFAULT, id, &err);
  EXPECT_EQ(err, "");
  return app;
}

// For testing purposes, we want to pretend there are only |app_type| apps on
// the system. This method removes the others.
void RemoveApps(apps::AppType app_type,
                Profile* profile,
                FakeAppListModelUpdater* model_updater) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  proxy->AppRegistryCache().ForEachApp(
      [&model_updater, &app_type](const apps::AppUpdate& update) {
        if (update.AppType() != app_type) {
          model_updater->RemoveItem(update.AppId(), /*is_uninstall=*/true);
        }
      });
}

void WaitForIconUpdates(ChromeAppListItem* item) {
  ASSERT_TRUE(item);
  do {
    content::RunAllTasksUntilIdle();
  } while (item->icon().isNull());
}

void VerifyIcon(const gfx::ImageSkia& src, const gfx::ImageSkia& dst) {
  ASSERT_FALSE(src.isNull());
  ASSERT_FALSE(dst.isNull());

  const std::vector<ui::ResourceScaleFactor>& scale_factors =
      ui::GetSupportedResourceScaleFactors();
  ASSERT_EQ(2U, scale_factors.size());

  for (const auto scale_factor : scale_factors) {
    const float scale = ui::GetScaleForResourceScaleFactor(scale_factor);
    ASSERT_TRUE(src.HasRepresentation(scale));
    ASSERT_TRUE(dst.HasRepresentation(scale));
    ASSERT_TRUE(
        gfx::test::AreBitmapsEqual(src.GetRepresentation(scale).GetBitmap(),
                                   dst.GetRepresentation(scale).GetBitmap()));
  }
}

void InitAppPosition(ChromeAppListItem* new_item) {
  if (new_item->position().IsValid())
    return;

  new_item->SetChromePosition(new_item->CalculateDefaultPositionForTest());
}

}  // namespace

class AppServiceAppModelBuilderTest : public AppListTestBase {
 public:
  AppServiceAppModelBuilderTest() {
    display::Screen::SetScreenInstance(&test_screen_);
  }

  ~AppServiceAppModelBuilderTest() override {
    display::Screen::SetScreenInstance(nullptr);
  }

  AppServiceAppModelBuilderTest(const AppServiceAppModelBuilderTest&) = delete;
  AppServiceAppModelBuilderTest& operator=(
      const AppServiceAppModelBuilderTest&) = delete;

  void TearDown() override {
    ResetBuilder();
    AppListTestBase::TearDown();
  }

 protected:
  void ResetBuilder() {
    scoped_callback_.reset();
    builder_.reset();
    controller_.reset();
    model_updater_.reset();
  }

  // Creates a new builder, destroying any existing one.
  void CreateBuilder(bool guest_mode) {
    ResetBuilder();  // Destroy any existing builder in the correct order.

    app_service_test_.UninstallAllApps(profile());
    testing_profile()->SetGuestSession(guest_mode);
    app_service_test_.SetUp(profile());
    // Wait for some default apps added to AppService.
    base::RunLoop().RunUntilIdle();
    model_updater_ = std::make_unique<FakeAppListModelUpdater>(
        /*profile=*/nullptr, /*reorder_delegate=*/nullptr);
    controller_ = std::make_unique<test::TestAppListControllerDelegate>();
    builder_ = std::make_unique<AppServiceAppModelBuilder>(controller_.get());
    scoped_callback_ = std::make_unique<
        AppServiceAppModelBuilder::ScopedAppPositionInitCallbackForTest>(
        builder_.get(), base::BindRepeating(&InitAppPosition));
    builder_->Initialize(nullptr, profile(), model_updater_.get());
  }

  apps::AppServiceTest app_service_test_;
  std::unique_ptr<
      AppServiceAppModelBuilder::ScopedAppPositionInitCallbackForTest>
      scoped_callback_;
  std::unique_ptr<AppServiceAppModelBuilder> builder_;
  std::unique_ptr<FakeAppListModelUpdater> model_updater_;
  std::unique_ptr<test::TestAppListControllerDelegate> controller_;
  display::test::TestScreen test_screen_;
};

class BuiltInAppTest : public AppServiceAppModelBuilderTest {
 public:
  // Don't call AppListTestBase::SetUp() - it's called from CreateBuilder().
  void SetUp() override {}

 protected:
  // Creates a new builder. Should be called only once for each test.
  // Calls `AppListTestBase::SetUp()`.
  void CreateBuilder(bool guest_mode) {
    AppListTestBase::SetUp(guest_mode);
    AppServiceAppModelBuilderTest::CreateBuilder(guest_mode);
    RemoveApps(apps::AppType::kBuiltIn, profile(), model_updater_.get());
  }
};

class ExtensionAppTest : public AppServiceAppModelBuilderTest {
 public:
  void SetUp() override {
    AppServiceAppModelBuilderTest::SetUp();

    preinstalled_apps_ = {"Hosted App", "Packaged App 1", "Packaged App 2"};
    CreateBuilder();
  }

 protected:
  // Creates a new builder, destroying any existing one.
  void CreateBuilder() {
    AppServiceAppModelBuilderTest::CreateBuilder(false /*guest_mode*/);
    RemoveApps(apps::AppType::kChromeApp, testing_profile(),
               model_updater_.get());
  }

  void GenerateExtensionAppIcon(const std::string app_id,
                                gfx::ImageSkia& output_image_skia) {
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile());
    ASSERT_TRUE(registry);
    const extensions::Extension* extension =
        registry->GetInstalledExtension(app_id);
    ASSERT_TRUE(extension);

    int size_in_dip =
        ash::SharedAppListConfig::instance().default_grid_icon_dimension();
    base::test::TestFuture<const gfx::Image&> image_future;
    extensions::ImageLoader::Get(profile())->LoadImageAtEveryScaleFactorAsync(
        extension, gfx::Size(size_in_dip, size_in_dip),
        image_future.GetCallback());
    output_image_skia =
        apps::CreateStandardIconImage(image_future.Take().AsImageSkia());
  }

  void GenerateExtensionAppCompressedIcon(const std::string app_id,
                                          std::vector<uint8_t>& result) {
    gfx::ImageSkia image_skia;
    GenerateExtensionAppIcon(app_id, image_skia);

    const float scale = 1.0;
    const gfx::ImageSkiaRep& image_skia_rep =
        image_skia.GetRepresentation(scale);
    ASSERT_EQ(image_skia_rep.scale(), scale);

    const SkBitmap& bitmap = image_skia_rep.GetBitmap();
    const bool discard_transparency = false;
    ASSERT_TRUE(gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, discard_transparency,
                                                  &result));
  }

  std::vector<std::string> preinstalled_apps_;
};

class WebAppBuilderTest : public AppServiceAppModelBuilderTest {
 public:
  void SetUp() override {
    AppServiceAppModelBuilderTest::SetUp();

    CreateBuilder();
  }

 protected:
  // Creates a new builder, destroying any existing one.
  void CreateBuilder() {
    AppServiceAppModelBuilderTest::CreateBuilder(false /*guest_mode*/);
    RemoveApps(apps::AppType::kWeb, testing_profile(), model_updater_.get());
  }

  std::string CreateWebApp(const std::string& app_name) {
    const GURL kAppUrl("https://example.com/");

    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(kAppUrl);
    web_app_info->title = base::UTF8ToUTF16(app_name);
    web_app_info->scope = kAppUrl;
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;

    return web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  }

  void GenerateWebAppIcon(const std::string& app_id,
                          gfx::ImageSkia& output_image_skia) {
    std::vector<int> icon_sizes_in_px;
    apps::ScaleToSize scale_to_size_in_px;
    int size_in_dip =
        ash::SharedAppListConfig::instance().default_grid_icon_dimension();
    for (const auto scale_factor : ui::GetSupportedResourceScaleFactors()) {
      int size_in_px = gfx::ScaleToFlooredSize(
                           gfx::Size(size_in_dip, size_in_dip),
                           ui::GetScaleForResourceScaleFactor(scale_factor))
                           .width();
      scale_to_size_in_px[ui::GetScaleForResourceScaleFactor(scale_factor)] =
          size_in_px;
      icon_sizes_in_px.emplace_back(size_in_px);
    }

    web_app::WebAppProvider* web_app_provider =
        web_app::WebAppProvider::GetForTest(profile());
    ASSERT_TRUE(web_app_provider);

    base::test::TestFuture<std::map<web_app::SquareSizePx, SkBitmap>>
        read_icons_future;
    web_app_provider->icon_manager().ReadIcons(
        app_id, web_app::IconPurpose::ANY, icon_sizes_in_px,
        read_icons_future.GetCallback());
    auto icon_bitmaps = read_icons_future.Take();
    for (auto [scale, size_px] : scale_to_size_in_px) {
      output_image_skia.AddRepresentation(
          gfx::ImageSkiaRep(icon_bitmaps[size_px], scale));
    }
    output_image_skia = gfx::ImageSkiaOperations::CreateMaskedImage(
        output_image_skia, apps::LoadMaskImage(scale_to_size_in_px));

    extensions::ChromeAppIcon::ApplyEffects(
        size_in_dip, extensions::ChromeAppIcon::ResizeFunction(),
        /*app_launchable=*/true, /*rounded_corners=*/true,
        extensions::ChromeAppIcon::Badge::kNone, &output_image_skia);
    for (const auto scale_factor : ui::GetSupportedResourceScaleFactors()) {
      // Force the icon to be loaded.
      output_image_skia.GetRepresentation(
          ui::GetScaleForResourceScaleFactor(scale_factor));
    }
  }

 private:
  base::test::ScopedCommandLine scoped_command_line_;
};

TEST_F(BuiltInAppTest, Build) {
  // The internal apps list is provided by GetInternalAppList() in
  // internal_app_metadata.cc. Only count the apps can display in launcher.
  std::string built_in_apps_name;
  CreateBuilder(false);
  EXPECT_EQ(GetNumberOfInternalAppsShowInLauncherForTest(&built_in_apps_name,
                                                         profile()),
            model_updater_->ItemCount());
  EXPECT_EQ(built_in_apps_name,
            base::JoinString(GetModelContent(model_updater_.get()), ","));
}

TEST_F(BuiltInAppTest, BuildGuestMode) {
  // The internal apps list is provided by GetInternalAppList() in
  // internal_app_metadata.cc. Only count the apps can display in launcher.
  std::string built_in_apps_name;
  CreateBuilder(true);
  EXPECT_EQ(GetNumberOfInternalAppsShowInLauncherForTest(&built_in_apps_name,
                                                         profile()),
            model_updater_->ItemCount());
  EXPECT_EQ(built_in_apps_name,
            base::JoinString(GetModelContent(model_updater_.get()), ","));
}

TEST_F(ExtensionAppTest, Build) {
  // The apps list would have 3 extension apps in the profile.
  EXPECT_EQ(kDefaultAppCount, model_updater_->ItemCount());
  EXPECT_EQ(preinstalled_apps_, GetModelContent(model_updater_.get()));
}

TEST_F(ExtensionAppTest, HideWebStore) {
  app_service_test_.SetUp(profile());

  // Install a "web store" app.
  scoped_refptr<extensions::Extension> store =
      MakeApp("webstore", "0.0", "http://google.com",
              std::string(extensions::kWebStoreAppId));
  service_->AddExtension(store.get());

  // Web store should be present in the model.
  FakeAppListModelUpdater model_updater1(/*profile=*/nullptr,
                                         /*reorder_delegate=*/nullptr);
  AppServiceAppModelBuilder builder1(controller_.get());
  auto scoped_callback1 = std::make_unique<
      AppServiceAppModelBuilder::ScopedAppPositionInitCallbackForTest>(
      &builder1, base::BindRepeating(&InitAppPosition));
  builder1.Initialize(nullptr, profile_.get(), &model_updater1);
  EXPECT_TRUE(model_updater1.FindItem(store->id()));

  // Activate the HideWebStoreIcon policy.
  profile_->GetPrefs()->SetBoolean(policy::policy_prefs::kHideWebStoreIcon,
                                   true);
  // Now the web store should not be present anymore.
  EXPECT_FALSE(model_updater1.FindItem(store->id()));

  // Build a new model; web store should NOT be present.
  FakeAppListModelUpdater model_updater2(/*profile=*/nullptr,
                                         /*reorder_delegate=*/nullptr);
  AppServiceAppModelBuilder builder2(controller_.get());
  auto scoped_callback2 = std::make_unique<
      AppServiceAppModelBuilder::ScopedAppPositionInitCallbackForTest>(
      &builder2, base::BindRepeating(&InitAppPosition));
  builder2.Initialize(nullptr, profile_.get(), &model_updater2);
  EXPECT_FALSE(model_updater2.FindItem(store->id()));

  // Deactivate the HideWebStoreIcon policy again.
  profile_->GetPrefs()->SetBoolean(policy::policy_prefs::kHideWebStoreIcon,
                                   false);
  // Now the web store should have appeared.
  EXPECT_TRUE(model_updater2.FindItem(store->id()));

  // Destroy scoped callbacks before model builders.
  scoped_callback1.reset();
  scoped_callback2.reset();
}

TEST_F(ExtensionAppTest, DisableAndEnable) {
  service_->DisableExtension(kHostedAppId,
                             extensions::disable_reason::DISABLE_USER_ACTION);
  EXPECT_EQ(preinstalled_apps_, GetModelContent(model_updater_.get()));

  service_->EnableExtension(kHostedAppId);
  EXPECT_EQ(preinstalled_apps_, GetModelContent(model_updater_.get()));
}

TEST_F(ExtensionAppTest, Uninstall) {
  service_->UninstallExtension(
      kPackagedApp2Id, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
  EXPECT_EQ((std::vector<std::string>{"Hosted App", "Packaged App 1"}),
            GetModelContent(model_updater_.get()));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ExtensionAppTest, UninstallTerminatedApp) {
  ASSERT_NE(nullptr, registry()->GetInstalledExtension(kPackagedApp2Id));

  // Simulate an app termination.
  service_->TerminateExtension(kPackagedApp2Id);

  service_->UninstallExtension(
      kPackagedApp2Id, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
  EXPECT_EQ((std::vector<std::string>{"Hosted App", "Packaged App 1"}),
            GetModelContent(model_updater_.get()));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ExtensionAppTest, Reinstall) {
  EXPECT_EQ(preinstalled_apps_, GetModelContent(model_updater_.get()));

  // Install kPackagedApp1Id again should not create a new entry.
  extensions::InstallTracker* tracker =
      extensions::InstallTrackerFactory::GetForBrowserContext(profile_.get());
  extensions::InstallObserver::ExtensionInstallParams params(
      kPackagedApp1Id, "", gfx::ImageSkia(), true, true);
  tracker->OnBeginExtensionInstall(params);

  EXPECT_EQ(preinstalled_apps_, GetModelContent(model_updater_.get()));
}

TEST_F(ExtensionAppTest, OrdinalPrefsChange) {
  AppSorting* sorting = ExtensionSystem::Get(profile_.get())->app_sorting();

  syncer::StringOrdinal package_app_page =
      sorting->GetPageOrdinal(kPackagedApp1Id);
  sorting->SetPageOrdinal(kHostedAppId, package_app_page.CreateBefore());
  // Old behavior: This would be "Hosted App,Packaged App 1,Packaged App 2"
  // New behavior: Sorting order doesn't change.
  EXPECT_EQ(preinstalled_apps_, GetModelContent(model_updater_.get()));

  syncer::StringOrdinal app1_ordinal =
      sorting->GetAppLaunchOrdinal(kPackagedApp1Id);
  syncer::StringOrdinal app2_ordinal =
      sorting->GetAppLaunchOrdinal(kPackagedApp2Id);
  sorting->SetPageOrdinal(kHostedAppId, package_app_page);
  sorting->SetAppLaunchOrdinal(kHostedAppId,
                               app1_ordinal.CreateBetween(app2_ordinal));
  // Old behavior: This would be "Packaged App 1,Hosted App,Packaged App 2"
  // New behavior: Sorting order doesn't change.
  EXPECT_EQ(preinstalled_apps_, GetModelContent(model_updater_.get()));
}

TEST_F(ExtensionAppTest, OnExtensionMoved) {
  AppSorting* sorting = ExtensionSystem::Get(profile_.get())->app_sorting();
  sorting->SetPageOrdinal(kHostedAppId,
                          sorting->GetPageOrdinal(kPackagedApp1Id));

  sorting->OnExtensionMoved(kHostedAppId, kPackagedApp1Id, kPackagedApp2Id);
  // Old behavior: This would be "Packaged App 1,Hosted App,Packaged App 2"
  // New behavior: Sorting order doesn't change.
  EXPECT_EQ(preinstalled_apps_, GetModelContent(model_updater_.get()));

  sorting->OnExtensionMoved(kHostedAppId, kPackagedApp2Id, std::string());
  // Old behavior: This would be restored to the default order.
  // New behavior: Sorting order still doesn't change.
  EXPECT_EQ(preinstalled_apps_, GetModelContent(model_updater_.get()));

  sorting->OnExtensionMoved(kHostedAppId, std::string(), kPackagedApp1Id);
  // Old behavior: This would be "Hosted App,Packaged App 1,Packaged App 2"
  // New behavior: Sorting order doesn't change.
  EXPECT_EQ(preinstalled_apps_, GetModelContent(model_updater_.get()));
}

TEST_F(ExtensionAppTest, InvalidOrdinal) {
  // Creates a no-ordinal case.
  AppSorting* sorting = ExtensionSystem::Get(profile_.get())->app_sorting();
  sorting->ClearOrdinals(kPackagedApp1Id);

  // Creates a corrupted ordinal case.
  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile_.get());
  prefs->UpdateExtensionPref(kHostedAppId, "page_ordinal",
                             base::Value("a corrupted ordinal"));

  // This should not assert or crash.
  CreateBuilder();
}

TEST_F(ExtensionAppTest, LoadIcon) {
  // Generate the source icon for comparing.
  gfx::ImageSkia src_image_skia;
  GenerateExtensionAppIcon(kPackagedApp1Id, src_image_skia);

  auto* item = model_updater_->FindItem(kPackagedApp1Id);
  item->LoadIcon();
  WaitForIconUpdates(item);

  VerifyIcon(src_image_skia, item->icon());
}

TEST_F(ExtensionAppTest, LoadCompressedIcon) {
  // Generate the source icon for comparing.
  std::vector<uint8_t> src_data;
  GenerateExtensionAppCompressedIcon(kPackagedApp1Id, src_data);

  apps::IconEffects icon_effects = apps::IconEffects::kCrOsStandardIcon;

  base::test::TestFuture<apps::IconValuePtr> dst_icon_future;
  apps::LoadIconFromExtension(
      apps::IconType::kCompressed,
      ash::SharedAppListConfig::instance().default_grid_icon_dimension(),
      profile(), kPackagedApp1Id, icon_effects, dst_icon_future.GetCallback());

  auto dst_icon = dst_icon_future.Take();
  ASSERT_TRUE(dst_icon);
  ASSERT_EQ(apps::IconType::kCompressed, dst_icon->icon_type);
  ASSERT_FALSE(dst_icon->is_placeholder_icon);
  ASSERT_FALSE(dst_icon->compressed.empty());

  ASSERT_EQ(src_data, dst_icon->compressed);
}

// This test adds a web app to the app list.
TEST_F(WebAppBuilderTest, WebAppList) {
  const std::string kAppName = "Web App";
  CreateWebApp(kAppName);

  app_service_test_.SetUp(profile_.get());
  RemoveApps(apps::AppType::kWeb, profile(), model_updater_.get());
  EXPECT_EQ(1u, model_updater_->ItemCount());
  EXPECT_EQ((std::vector<std::string>{kAppName}),
            GetModelContent(model_updater_.get()));
}

TEST_F(WebAppBuilderTest, LoadGeneratedIcon) {
  const std::string kAppName = "Web App";
  const std::string app_id = CreateWebApp(kAppName);

  // Generate the source icon for comparing.
  gfx::ImageSkia src_image_skia;
  GenerateWebAppIcon(app_id, src_image_skia);

  auto* item = model_updater_->FindItem(app_id);
  item->LoadIcon();
  WaitForIconUpdates(item);

  VerifyIcon(src_image_skia, item->icon());
}

class WebAppBuilderDemoModeTest : public WebAppBuilderTest {
 protected:
  ~WebAppBuilderDemoModeTest() override = default;

  void SetUp() override {
    WebAppBuilderTest::SetUp();
    CreateBuilder();

    // Fake Demo Mode.
    cros_settings_test_helper().InstallAttributes()->SetDemoMode();
    demo_mode_test_helper_ = std::make_unique<ash::DemoModeTestHelper>();
    demo_mode_test_helper_->InitializeSession();

    app_service_test_.SetUp(profile_.get());
    // Wait for some default apps added to AppService.
    base::RunLoop().RunUntilIdle();
    RemoveApps(apps::AppType::kWeb, profile(), model_updater_.get());
  }

  void TearDown() override {
    demo_mode_test_helper_.reset();

    WebAppBuilderTest::TearDown();
  }

 private:
  std::unique_ptr<ash::DemoModeTestHelper> demo_mode_test_helper_;
};

// This test adds a web app to the app list for demo mode when online.
TEST_F(WebAppBuilderDemoModeTest, WebAppListOnline) {
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);

  const std::string kAppName = "https://test.com";
  CreateWebApp(kAppName);

  EXPECT_EQ(1u, model_updater_->ItemCount());
}

// This test adds a web app to the app list for demo mode when offline.
TEST_F(WebAppBuilderDemoModeTest, WebAppListOffline) {
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);

  const std::string kAppName = "https://test.com";
  CreateWebApp(kAppName);

  EXPECT_EQ(0u, model_updater_->ItemCount());
}

class CrostiniAppTest : public AppServiceAppModelBuilderTest {
 public:
  CrostiniAppTest() = default;
  ~CrostiniAppTest() override {}

  CrostiniAppTest(const CrostiniAppTest&) = delete;
  CrostiniAppTest& operator=(const CrostiniAppTest&) = delete;

  void SetUp() override {
    ash::CiceroneClient::InitializeFake();
    ash::ConciergeClient::InitializeFake();
    ash::SeneschalClient::InitializeFake();
    AppServiceAppModelBuilderTest::SetUp();
    test_helper_ = std::make_unique<CrostiniTestHelper>(testing_profile());
    test_helper_->ReInitializeAppServiceIntegration();
    CreateBuilder();
  }

  void TearDown() override {
    ResetBuilder();
    test_helper_.reset();
    AppListTestBase::TearDown();

    // |profile_| is initialized in AppListTestBase::SetUp but not destroyed in
    // the ::TearDown method, but we need it to go away before shutting down
    // DBusThreadManager to ensure all keyed services that might rely on DBus
    // clients are destroyed.
    profile_.reset();
    ash::SeneschalClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::CiceroneClient::Shutdown();
  }

 protected:
  AppListModelUpdater* GetModelUpdater() const {
    return sync_service_->GetModelUpdater();
  }

  size_t GetModelItemCount() const {
    // Pump the Mojo IPCs.
    base::RunLoop().RunUntilIdle();
    return sync_service_->GetModelUpdater()->ItemCount();
  }

  std::vector<const ChromeAppListItem*> GetAllApps() const {
    return GetModelUpdater()->GetItems();
  }

  // For testing purposes, we want to pretend there are only crostini apps on
  // the system. This method removes the others.
  void RemoveNonCrostiniApps() {
    std::vector<std::string> existing_item_ids;
    for (const auto& pair : sync_service_->sync_items()) {
      existing_item_ids.emplace_back(pair.first);
    }
    for (const std::string& id : existing_item_ids) {
      if (id == ash::kCrostiniFolderId) {
        continue;
      }
      sync_service_->RemoveItem(id, /*is_uninstall=*/false);
    }
  }

  void CreateBuilder() {
    model_updater_factory_scope_ =
        AppListSyncableService::SetScopedModelUpdaterFactoryForTest(
            base::BindRepeating(
                [](Profile* profile,
                   reorder::AppListReorderDelegate* reorder_delegate)
                    -> std::unique_ptr<AppListModelUpdater> {
                  return std::make_unique<FakeAppListModelUpdater>(
                      profile, reorder_delegate);
                },
                profile()));
    // The AppListSyncableService creates the CrostiniAppModelBuilder.
    sync_service_ = std::make_unique<AppListSyncableService>(profile());
    RemoveNonCrostiniApps();
  }

  void ResetBuilder() {
    sync_service_.reset();
    model_updater_factory_scope_.reset();
  }

  guest_os::GuestOsRegistryService* RegistryService() {
    return guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile());
  }

  std::string TerminalAppName() {
    return l10n_util::GetStringUTF8(IDS_CROSTINI_TERMINAL_APP_NAME);
  }

  std::unique_ptr<AppListSyncableService> sync_service_;
  std::unique_ptr<CrostiniTestHelper> test_helper_;

 private:
  std::unique_ptr<base::ScopedClosureRunner> model_updater_factory_scope_;
};

// Test that the Terminal app is only shown when Crostini is enabled
TEST_F(CrostiniAppTest, EnableAndDisableCrostini) {
  // Reset things so we start with Crostini not enabled.
  ResetBuilder();
  test_helper_.reset();
  test_helper_ = std::make_unique<CrostiniTestHelper>(
      testing_profile(), /*enable_crostini=*/false);
  CreateBuilder();

  EXPECT_EQ(0u, GetModelItemCount());

  CrostiniTestHelper::EnableCrostini(testing_profile());
  EXPECT_THAT(GetAllApps(), testing::IsEmpty());
  CrostiniTestHelper::DisableCrostini(testing_profile());
  EXPECT_THAT(GetAllApps(), testing::IsEmpty());
}

TEST_F(CrostiniAppTest, AppInstallation) {
  EXPECT_EQ(0u, GetModelItemCount());

  test_helper_->SetupDummyApps();

  EXPECT_THAT(GetAllApps(),
              testing::UnorderedElementsAre(
                  IsChromeApp(_, kDummyApp1Name, ash::kCrostiniFolderId),
                  IsChromeApp(_, kDummyApp2Name, ash::kCrostiniFolderId),
                  IsChromeApp(ash::kCrostiniFolderId, _, "")));

  test_helper_->AddApp(
      CrostiniTestHelper::BasicApp(kBananaAppId, kBananaAppName));
  EXPECT_THAT(GetAllApps(),
              testing::UnorderedElementsAre(
                  IsChromeApp(_, kDummyApp1Name, ash::kCrostiniFolderId),
                  IsChromeApp(_, kDummyApp2Name, ash::kCrostiniFolderId),
                  IsChromeApp(_, kBananaAppName, ash::kCrostiniFolderId),
                  IsChromeApp(ash::kCrostiniFolderId, _, "")));
}

// Test that the app model builder correctly picks up changes to existing apps.
TEST_F(CrostiniAppTest, UpdateApps) {
  test_helper_->SetupDummyApps();

  // 3 items: two dummy apps and the Crostini folder.
  EXPECT_EQ(3u, GetModelItemCount());

  // Setting NoDisplay to true should hide an app.
  vm_tools::apps::App dummy1 = test_helper_->GetApp(0);
  dummy1.set_no_display(true);
  test_helper_->AddApp(dummy1);
  EXPECT_THAT(
      GetAllApps(),
      testing::UnorderedElementsAre(
          IsChromeApp(CrostiniTestHelper::GenerateAppId(kDummyApp2Name), _, _),
          IsChromeApp(ash::kCrostiniFolderId, _, "")));

  // Setting NoDisplay to false should unhide an app.
  dummy1.set_no_display(false);
  test_helper_->AddApp(dummy1);
  EXPECT_THAT(
      GetAllApps(),
      testing::UnorderedElementsAre(
          IsChromeApp(CrostiniTestHelper::GenerateAppId(kDummyApp1Name), _, _),
          IsChromeApp(CrostiniTestHelper::GenerateAppId(kDummyApp2Name), _, _),
          IsChromeApp(ash::kCrostiniFolderId, _, "")));

  // Changes to app names should be detected.
  vm_tools::apps::App dummy2 =
      CrostiniTestHelper::BasicApp(kDummyApp2Id, kAppNewName);
  test_helper_->AddApp(dummy2);
  EXPECT_THAT(GetAllApps(),
              testing::UnorderedElementsAre(
                  IsChromeApp(CrostiniTestHelper::GenerateAppId(kDummyApp1Name),
                              kDummyApp1Name, _),
                  IsChromeApp(CrostiniTestHelper::GenerateAppId(kDummyApp2Name),
                              kAppNewName, _),
                  IsChromeApp(ash::kCrostiniFolderId, _, "")));
}

// Test that the app model builder handles removed apps
TEST_F(CrostiniAppTest, RemoveApps) {
  test_helper_->SetupDummyApps();
  // 3 items: two dummy apps and the Crostini folder.
  EXPECT_EQ(3u, GetModelItemCount());

  // Remove dummy1
  test_helper_->RemoveApp(0);
  EXPECT_EQ(2u, GetModelItemCount());

  // Remove dummy2
  test_helper_->RemoveApp(0);
  EXPECT_EQ(0u, GetModelItemCount());
}

// Tests that the crostini folder is created with the correct parameters.
TEST_F(CrostiniAppTest, CreatesFolder) {
  test_helper_->SetupDummyApps();
  EXPECT_THAT(GetAllApps(),
              testing::UnorderedElementsAre(
                  IsChromeApp(_, kDummyApp1Name, ash::kCrostiniFolderId),
                  IsChromeApp(_, kDummyApp2Name, ash::kCrostiniFolderId),
                  testing::AllOf(
                      IsChromeApp(ash::kCrostiniFolderId, kRootFolderName, ""),
                      IsSystemFolder())));
}

// Test that the Terminal app is removed when Crostini is disabled.
TEST_F(CrostiniAppTest, DisableCrostini) {
  test_helper_->SetupDummyApps();
  // 3 items: two dummy apps and the Crostini folder.
  EXPECT_EQ(3u, GetModelItemCount());

  // The uninstall flow removes all apps before setting the CrostiniEnabled pref
  // to false, so we need to do that explicitly too.
  RegistryService()->ClearApplicationList(guest_os::VmType::TERMINA,
                                          crostini::kCrostiniDefaultVmName, "");
  CrostiniTestHelper::DisableCrostini(testing_profile());
  EXPECT_EQ(0u, GetModelItemCount());
}

class PluginVmAppTest : public testing::Test {
 public:
  void SetUp() override {
    testing_profile_ = std::make_unique<TestingProfile>();
    web_app::FakeWebAppProvider::Get(testing_profile_.get())->Start();
    test_helper_ = std::make_unique<PluginVmTestHelper>(testing_profile_.get());
    // We need to call this before creating the builder, otherwise
    // |PluginVmApps| is disabled forever.
    test_helper_->SetUserRequirementsToAllowPluginVm();

    CreateBuilder();
  }

  void TearDown() override { ResetBuilder(); }

 protected:
  // Required to ensure that the Plugin VM manager can be accessed in order to
  // retrieve permissions.
  struct ScopedDBusClients {
    ScopedDBusClients() {
      ash::CiceroneClient::InitializeFake();
      ash::ConciergeClient::InitializeFake();
      ash::SeneschalClient::InitializeFake();
    }
    ~ScopedDBusClients() {
      ash::SeneschalClient::Shutdown();
      ash::ConciergeClient::Shutdown();
      ash::CiceroneClient::Shutdown();
    }
  } dbus_clients_;

  // Destroys any existing builder in the correct order.
  void ResetBuilder() {
    scoped_callback_.reset();
    builder_.reset();
    controller_.reset();
    model_updater_.reset();
  }

  // Creates a new builder, destroying any existing one.
  void CreateBuilder() {
    ResetBuilder();

    app_service_test_.UninstallAllApps(testing_profile_.get());
    testing_profile_->SetGuestSession(false);
    app_service_test_.SetUp(testing_profile_.get());
    model_updater_ = std::make_unique<FakeAppListModelUpdater>(
        /*profile=*/nullptr, /*reorder_delegate=*/nullptr);
    controller_ = std::make_unique<test::TestAppListControllerDelegate>();
    builder_ = std::make_unique<AppServiceAppModelBuilder>(controller_.get());
    scoped_callback_ = std::make_unique<
        AppServiceAppModelBuilder::ScopedAppPositionInitCallbackForTest>(
        builder_.get(), base::BindRepeating(&InitAppPosition));
    builder_->Initialize(nullptr, testing_profile_.get(), model_updater_.get());

    RemoveApps(apps::AppType::kPluginVm, testing_profile_.get(),
               model_updater_.get());
  }

  void AllowPluginVm() {
    // We cannot call test_helper_.AllowPluginVm() because we have called
    // SetUserRequirementsToAllowPluginVm()
    test_helper_->EnablePluginVmFeature();
    test_helper_->EnterpriseEnrollDevice();
    test_helper_->SetPolicyRequirementsToAllowPluginVm();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<PluginVmTestHelper> test_helper_;

  apps::AppServiceTest app_service_test_;
  std::unique_ptr<AppServiceAppModelBuilder> builder_;
  std::unique_ptr<
      AppServiceAppModelBuilder::ScopedAppPositionInitCallbackForTest>
      scoped_callback_;
  std::unique_ptr<FakeAppListModelUpdater> model_updater_;
  std::unique_ptr<test::TestAppListControllerDelegate> controller_;
};

TEST_F(PluginVmAppTest, PluginVmDisabled) {
  EXPECT_FALSE(
      plugin_vm::PluginVmFeatures::Get()->IsAllowed(testing_profile_.get()));
  EXPECT_THAT(GetModelContent(model_updater_.get()), testing::IsEmpty());
}

TEST_F(PluginVmAppTest, EnableAndDisablePluginVm) {
  EXPECT_THAT(GetModelContent(model_updater_.get()), testing::IsEmpty());

  AllowPluginVm();

  EXPECT_EQ(std::vector<std::string>{l10n_util::GetStringUTF8(
                IDS_PLUGIN_VM_APP_NAME)},
            GetModelContent(model_updater_.get()));

  testing_profile_->ScopedCrosSettingsTestHelper()->SetBoolean(
      ash::kPluginVmAllowed, false);

  EXPECT_THAT(GetModelContent(model_updater_.get()), testing::IsEmpty());
}

TEST_F(PluginVmAppTest, PluginVmEnabled) {
  AllowPluginVm();

  // Reset the AppModelBuilder, so that it is created in a state where
  // Plugin VM was enabled.
  CreateBuilder();

  EXPECT_EQ(std::vector<std::string>{l10n_util::GetStringUTF8(
                IDS_PLUGIN_VM_APP_NAME)},
            GetModelContent(model_updater_.get()));
}

}  // namespace app_list
