// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_syncable_service.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service_constants.h"
#include "chrome/browser/themes/test/theme_service_changed_waiter.h"
#include "chrome/browser/themes/theme_helper.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/themes/theme_service_test_utils.h"
#include "chrome/browser/themes/theme_service_utils.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/features.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "components/sync/test/sync_change_processor_wrapper_for_test.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/pending_extension_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/users/user_manager_delegate_impl.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#endif

using std::string;

namespace {

using theme_service::test::MakeThemeChangeList;
using theme_service::test::MakeThemeDataList;
using theme_service::test::MakeThemeExtension;

static const char* kCustomThemeId = "abcdefghijklmnopabcdefghijklmnop";
static const char kCustomThemeName[] = "name";
static const char kCustomThemeUrl[] = "http://update.url/foo";

#if BUILDFLAG(IS_WIN)
const base::FilePath::CharType kExtensionFilePath[] =
    FILE_PATH_LITERAL("c:\\foo");
#elif BUILDFLAG(IS_POSIX)
const base::FilePath::CharType kExtensionFilePath[] = FILE_PATH_LITERAL("/oo");
#else
#error "Unknown platform"
#endif

constexpr char kTestUrl[] = "https://www.foo.com";

const char kThemePrefMigrationAlreadyMigratedHistogram[] =
    "Theme.ThemePrefMigration.AlreadyMigrated";
const char kThemePrefMigrationMigratedPrefHistogram[] =
    "Theme.ThemePrefMigration.MigratedPref";
const char kThemePrefMigrationIncomingSyncingPrefAppliedHistogram[] =
    "Theme.ThemePrefMigration.IncomingSyncingPrefApplied";

MATCHER_P2(DictionaryValuePtrHas, key, value, "") {
  return testing::ExplainMatchResult(
      testing::Pointee(base::test::DictionaryHasValue(key, base::Value(value))),
      arg, result_listener);
}

const ThemeHelper& GetThemeHelper() {
  static base::NoDestructor<std::unique_ptr<ThemeHelper>> theme_helper(
      std::make_unique<ThemeHelper>());
  return **theme_helper;
}

class FakeThemeService : public ThemeService {
 public:
  FakeThemeService() : ThemeService(nullptr, GetThemeHelper()) {}

  // ThemeService:
  void DoSetTheme(const extensions::Extension* extension,
                  bool suppress_infobar) override {
    is_dirty_ = true;
    theme_extension_ = extension;
    using_system_theme_ = false;
    using_default_theme_ = false;
    using_policy_theme_ = false;
    might_show_infobar_ = !suppress_infobar;
    color_ = 0;
  }

  void BuildAutogeneratedThemeFromColor(SkColor color) override {
    is_dirty_ = true;
    color_ = color;
    theme_extension_.reset();
    using_system_theme_ = false;
    using_default_theme_ = false;
    using_policy_theme_ = false;
  }

  void UseTheme(ui::SystemTheme system_theme) override {
    if (system_theme == ui::SystemTheme::kDefault) {
      UseDefaultTheme();
    } else {
      UseSystemTheme();
    }
  }

  void UseDefaultTheme() override {
    is_dirty_ = true;
    using_default_theme_ = true;
    using_system_theme_ = false;
    using_policy_theme_ = false;
    theme_extension_.reset();
    color_ = 0;
  }

  void UseSystemTheme() override {
    is_dirty_ = true;
    using_system_theme_ = true;
    using_default_theme_ = false;
    using_policy_theme_ = false;
    theme_extension_.reset();
    color_ = 0;
  }

  void BuildAutogeneratedPolicyTheme() override {
    is_dirty_ = true;
    color_ = SkColorSetRGB(100, 100, 0);
    theme_extension_.reset();
    using_system_theme_ = false;
    using_default_theme_ = false;
    using_policy_theme_ = true;
  }

  bool IsSystemThemeDistinctFromDefaultTheme() const override {
    return distinct_from_default_theme_;
  }

  void set_distinct_from_default_theme(bool is_distinct) {
    distinct_from_default_theme_ = is_distinct;
  }

  bool UsingDefaultTheme() const override { return using_default_theme_; }

  bool UsingSystemTheme() const override { return using_system_theme_; }

  bool UsingExtensionTheme() const override { return !!theme_extension_; }

  bool UsingAutogeneratedTheme() const override { return color_ != 0; }

  bool UsingPolicyTheme() const override { return using_policy_theme_; }

  string GetThemeID() const override {
    return UsingExtensionTheme() ? theme_extension_->id() : std::string();
  }

  SkColor GetAutogeneratedThemeColor() const override { return color_; }

  bool GetIsGrayscale() const override { return false; }

  ThemeService::BrowserColorScheme GetBrowserColorScheme() const override {
    return ThemeService::BrowserColorScheme::kSystem;
  }

  const extensions::Extension* theme_extension() const {
    return theme_extension_.get();
  }

  bool is_dirty() const { return is_dirty_; }

  void MarkClean() { is_dirty_ = false; }

  bool might_show_infobar() const { return might_show_infobar_; }

 private:
  bool using_system_theme_ = false;
  bool using_default_theme_ = false;
  bool using_policy_theme_ = false;
  bool distinct_from_default_theme_ = false;
  scoped_refptr<const extensions::Extension> theme_extension_;
  bool is_dirty_ = false;
  bool might_show_infobar_ = false;
  SkColor color_;
};

}  // namespace

// ThemeSyncableServiceTest ----------------------------------------------------

class ThemeSyncableServiceTest : public testing::Test,
                                 public ThemeSyncableService::Observer {
 protected:
  ThemeSyncableServiceTest() : fake_theme_service_(nullptr) {}

  ~ThemeSyncableServiceTest() override = default;

  void SetUp() override {
    // Setting a matching update URL is necessary to make the test theme
    // considered syncable.
    extension_test_util::SetGalleryUpdateURL(GURL(kCustomThemeUrl));

    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        ThemeServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<FakeThemeService>();
        }));
    profile_ = builder.Build();
    fake_theme_service_ = static_cast<FakeThemeService*>(
        ThemeServiceFactory::GetForProfile(profile_.get()));
    theme_sync_service_ = std::make_unique<ThemeSyncableService>(
        profile_.get(), fake_theme_service_);
    theme_sync_service_->AddObserver(this);
    fake_change_processor_ =
        std::make_unique<syncer::FakeSyncChangeProcessor>();
    SetUpExtension();
  }

  void TearDown() override {
    theme_sync_service_.reset();
    profile_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void SetUpExtension() {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    extensions::TestExtensionSystem* test_ext_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile_.get()));
    extensions::ExtensionService* service =
        test_ext_system->CreateExtensionService(
            &command_line, base::FilePath(kExtensionFilePath), false);
    auto* registrar = extensions::ExtensionRegistrar::Get(profile_.get());
    EXPECT_TRUE(registrar->extensions_enabled());
    service->Init();
    base::RunLoop().RunUntilIdle();

    // Create and add custom theme extension so the ThemeSyncableService can
    // find it.
    theme_extension_ = MakeThemeExtension(base::FilePath(kExtensionFilePath),
                                          kCustomThemeId, kCustomThemeName,
                                          GetThemeLocation(), kCustomThemeUrl);
    extensions::ExtensionPrefs::Get(profile_.get())
        ->AddGrantedPermissions(theme_extension_->id(),
                                extensions::PermissionSet());
    registrar->AddExtension(theme_extension_);
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile_.get());
    ASSERT_EQ(1u, registry->enabled_extensions().size());
  }

  // Overridden in PolicyInstalledThemeTest below.
  virtual extensions::mojom::ManifestLocation GetThemeLocation() {
    return extensions::mojom::ManifestLocation::kInternal;
  }

  void OnThemeSyncStarted(ThemeSyncableService::ThemeSyncState state) override {
    state_ = state;
  }

  bool HasThemeSyncStarted() { return state_ != std::nullopt; }

  bool HasThemeSyncTriggeredExtensionInstallation() {
    return state_ && *state_ == ThemeSyncableService::ThemeSyncState::
                                    kWaitingForExtensionInstallation;
  }

  // Needed for setting up extension service.
  content::BrowserTaskEnvironment task_environment_;

#if BUILDFLAG(IS_CHROMEOS)
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  user_manager::ScopedUserManager user_manager_{
      std::make_unique<user_manager::UserManagerImpl>(
          std::make_unique<ash::UserManagerDelegateImpl>(),
          g_browser_process->local_state(),
          ash::CrosSettings::Get())};
#endif

  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<FakeThemeService, DanglingUntriaged> fake_theme_service_;
  scoped_refptr<extensions::Extension> theme_extension_;
  std::unique_ptr<ThemeSyncableService> theme_sync_service_;
  std::unique_ptr<syncer::FakeSyncChangeProcessor> fake_change_processor_;
  std::optional<ThemeSyncableService::ThemeSyncState> state_;
};

TEST_F(ThemeSyncableServiceTest, AreThemeSpecificsEquivalent) {
  sync_pb::ThemeSpecifics a, b;
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, true));

  // Custom vs. non-custom.

  a.set_use_custom_theme(true);
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, true));

  a.set_use_custom_theme(false);
  b.set_use_custom_theme(true);
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, true));

  // Custom theme equality for extensions.
  a.set_use_custom_theme(true);
  b.set_use_custom_theme(true);
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, true));

  a.set_custom_theme_id("id");
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, true));

  b.set_custom_theme_id("id");
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, true));

  a.set_custom_theme_update_url("http://update.url");
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, true));

  a.set_custom_theme_name("name");
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, true));

  // Theme equality for autogenerated.
  a.set_use_custom_theme(true);
  b.set_use_custom_theme(false);
  b.mutable_autogenerated_color_theme()->set_color(SkColorSetRGB(0, 0, 0));
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, true));

  b.set_use_custom_theme(true);
  b.clear_autogenerated_color_theme();
  a.set_use_custom_theme(false);
  a.mutable_autogenerated_color_theme()->set_color(SkColorSetRGB(0, 0, 0));
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, true));

  b.set_use_custom_theme(false);
  b.mutable_autogenerated_color_theme()->set_color(SkColorSetRGB(0, 0, 100));
  a.mutable_autogenerated_color_theme()->set_color(SkColorSetRGB(0, 0, 100));
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, true));

  b.mutable_autogenerated_color_theme()->set_color(SkColorSetRGB(0, 0, 200));
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, true));

  // No theme equality.

  a.clear_autogenerated_color_theme();
  b.clear_autogenerated_color_theme();
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, true));

  a.set_use_system_theme_by_default(true);
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, true));

  b.set_use_system_theme_by_default(true);
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));
  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, true));
}

TEST_F(ThemeSyncableServiceTest, SetCurrentThemeDefaultTheme) {
  // Set up theme service to use custom theme.
  fake_theme_service_->SetTheme(theme_extension_.get());

  std::optional<syncer::ModelError> error =
      theme_sync_service_->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(sync_pb::ThemeSpecifics()),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor_.get())));
  EXPECT_TRUE(HasThemeSyncStarted());
  EXPECT_FALSE(error.has_value()) << error.value().message();
  EXPECT_FALSE(fake_theme_service_->UsingDefaultTheme());
  EXPECT_EQ(fake_theme_service_->theme_extension(), theme_extension_.get());
}

TEST_F(ThemeSyncableServiceTest, SetCurrentThemeSystemTheme) {
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_system_theme_by_default(true);

  // Set up theme service to use custom theme.
  fake_theme_service_->SetTheme(theme_extension_.get());
  std::optional<syncer::ModelError> error =
      theme_sync_service_->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor_.get())));
  EXPECT_TRUE(HasThemeSyncStarted());
  EXPECT_FALSE(error.has_value()) << error.value().message();
  EXPECT_FALSE(fake_theme_service_->UsingSystemTheme());
  EXPECT_EQ(fake_theme_service_->theme_extension(), theme_extension_.get());
}

TEST_F(ThemeSyncableServiceTest, SetCurrentThemeCustomTheme_Extension) {
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(true);
  theme_specifics.set_custom_theme_id(theme_extension_->id());
  theme_specifics.set_custom_theme_name(kCustomThemeName);
  theme_specifics.set_custom_theme_update_url(kCustomThemeUrl);

  // Set up theme service to use default theme.
  fake_theme_service_->UseDefaultTheme();
  std::optional<syncer::ModelError> error =
      theme_sync_service_->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor_.get())));
  EXPECT_TRUE(HasThemeSyncStarted());
  EXPECT_FALSE(HasThemeSyncTriggeredExtensionInstallation());
  EXPECT_FALSE(error.has_value()) << error.value().message();
  EXPECT_EQ(fake_theme_service_->theme_extension(), theme_extension_.get());
}

TEST_F(ThemeSyncableServiceTest, SetCurrentThemeCustomTheme_Extension_Install) {
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(true);
  // Use an arbitrary id (such an extension is not installed, yet).
  theme_specifics.set_custom_theme_id("fake_extension_id");
  theme_specifics.set_custom_theme_name(kCustomThemeName);
  theme_specifics.set_custom_theme_update_url(kCustomThemeUrl);

  // Set up theme service to use default theme.
  fake_theme_service_->UseDefaultTheme();
  std::optional<syncer::ModelError> error =
      theme_sync_service_->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor_.get())));
  EXPECT_TRUE(HasThemeSyncStarted());
  EXPECT_FALSE(error.has_value()) << error.value().message();
  // The theme is not installed yet and thus, the default theme is still used.
  EXPECT_TRUE(fake_theme_service_->UsingDefaultTheme());
  EXPECT_TRUE(HasThemeSyncTriggeredExtensionInstallation());
  EXPECT_TRUE(extensions::PendingExtensionManager::Get(profile_.get())
                  ->HasPendingExtensionFromSync());
}

TEST_F(ThemeSyncableServiceTest, SetCurrentThemeCustomTheme_Autogenerated) {
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(false);
  theme_specifics.mutable_autogenerated_color_theme()->set_color(
      SkColorSetRGB(0, 0, 100));

  // Set up theme service to use default theme.
  fake_theme_service_->UseDefaultTheme();
  std::optional<syncer::ModelError> error =
      theme_sync_service_->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor_.get())));
  EXPECT_TRUE(HasThemeSyncStarted());
  EXPECT_FALSE(error.has_value()) << error.value().message();
  EXPECT_EQ(fake_theme_service_->GetAutogeneratedThemeColor(),
            SkColorSetRGB(0, 0, 100));
}

TEST_F(ThemeSyncableServiceTest, DontResetThemeWhenSpecificsAreEqual) {
  // Set up theme service to use default theme and expect no changes.
  fake_theme_service_->UseDefaultTheme();
  fake_theme_service_->MarkClean();
  std::optional<syncer::ModelError> error =
      theme_sync_service_->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(sync_pb::ThemeSpecifics()),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor_.get())));
  EXPECT_TRUE(HasThemeSyncStarted());
  EXPECT_FALSE(error.has_value()) << error.value().message();
  EXPECT_FALSE(fake_theme_service_->is_dirty());
}

TEST_F(ThemeSyncableServiceTest, GetAllSyncDataForTesting_Extension) {
  // Set up theme service to use custom theme.
  fake_theme_service_->SetTheme(theme_extension_.get());

  syncer::SyncDataList data_list =
      theme_sync_service_->GetAllSyncDataForTesting(syncer::THEMES);

  ASSERT_EQ(1u, data_list.size());
  const sync_pb::ThemeSpecifics& theme_specifics =
      data_list[0].GetSpecifics().theme();
  EXPECT_TRUE(theme_specifics.use_custom_theme());
  EXPECT_EQ(theme_extension_->id(), theme_specifics.custom_theme_id());
  EXPECT_EQ(theme_extension_->name(), theme_specifics.custom_theme_name());
  EXPECT_EQ(
      extensions::ManifestURL::GetUpdateURL(theme_extension_.get()).spec(),
      theme_specifics.custom_theme_update_url());
}

TEST_F(ThemeSyncableServiceTest, GetAllSyncDataForTesting_Autogenerated) {
  // Set up theme service to use autogenerated theme.
  fake_theme_service_->BuildAutogeneratedThemeFromColor(
      SkColorSetRGB(0, 0, 100));

  syncer::SyncDataList data_list =
      theme_sync_service_->GetAllSyncDataForTesting(syncer::THEMES);

  ASSERT_EQ(1u, data_list.size());
  const sync_pb::ThemeSpecifics& theme_specifics =
      data_list[0].GetSpecifics().theme();
  EXPECT_FALSE(theme_specifics.use_custom_theme());
  EXPECT_EQ(fake_theme_service_->GetAutogeneratedThemeColor(),
            theme_specifics.autogenerated_color_theme().color());
}

TEST_F(ThemeSyncableServiceTest, GetAllSyncDataForTesting_Policy) {
  // Set up theme service to use policy theme.
  fake_theme_service_->BuildAutogeneratedPolicyTheme();

  // Themes applied through policy shouldn't be synced.
  syncer::SyncDataList data_list =
      theme_sync_service_->GetAllSyncDataForTesting(syncer::THEMES);
  ASSERT_EQ(0u, data_list.size());
}

TEST_F(ThemeSyncableServiceTest, ProcessSyncThemeChange_Extension) {
  // Set up theme service to use default theme.
  fake_theme_service_->UseDefaultTheme();
  fake_theme_service_->MarkClean();

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service_->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(sync_pb::ThemeSpecifics()),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor_.get())));
  EXPECT_FALSE(error.has_value()) << error.value().message();
  // Don't expect theme change initially because specifics are equal.
  EXPECT_FALSE(fake_theme_service_->is_dirty());

  // Change specifics to use custom theme and update.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(true);
  theme_specifics.set_custom_theme_id(theme_extension_->id());
  theme_specifics.set_custom_theme_name(kCustomThemeName);
  theme_specifics.set_custom_theme_update_url(kCustomThemeUrl);
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_theme()->CopyFrom(theme_specifics);
  syncer::SyncChangeList change_list;
  change_list.emplace_back(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      syncer::SyncData::CreateRemoteData(
          entity_specifics, syncer::ClientTagHash::FromHashed("unused")));
  std::optional<syncer::ModelError> process_error =
      theme_sync_service_->ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_FALSE(process_error.has_value()) << process_error.value().message();
  EXPECT_EQ(fake_theme_service_->theme_extension(), theme_extension_.get());
  // Don't show an infobar for theme installation. Regression test for
  // crbug.com/731688
  EXPECT_FALSE(fake_theme_service_->might_show_infobar());
}

TEST_F(ThemeSyncableServiceTest, ProcessSyncThemeChange_Autogenerated) {
  // Set up theme service to use default theme.
  fake_theme_service_->UseDefaultTheme();
  fake_theme_service_->MarkClean();

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service_->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(sync_pb::ThemeSpecifics()),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor_.get())));
  EXPECT_FALSE(error.has_value()) << error.value().message();
  // Don't expect theme change initially because specifics are equal.
  EXPECT_FALSE(fake_theme_service_->is_dirty());

  // Change specifics to use custom theme and update.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(false);
  theme_specifics.mutable_autogenerated_color_theme()->set_color(
      SkColorSetRGB(0, 0, 100));
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_theme()->CopyFrom(theme_specifics);
  syncer::SyncChangeList change_list;
  change_list.emplace_back(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      syncer::SyncData::CreateRemoteData(
          entity_specifics, syncer::ClientTagHash::FromHashed("unused")));
  std::optional<syncer::ModelError> process_error =
      theme_sync_service_->ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_FALSE(process_error.has_value()) << process_error.value().message();
  EXPECT_EQ(fake_theme_service_->GetAutogeneratedThemeColor(),
            SkColorSetRGB(0, 0, 100));
}

TEST_F(ThemeSyncableServiceTest, OnThemeChangeByUser_Extension) {
  // Set up theme service to use default theme.
  fake_theme_service_->UseDefaultTheme();

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service_->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(sync_pb::ThemeSpecifics()),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor_.get())));
  EXPECT_FALSE(error.has_value()) << error.value().message();
  const syncer::SyncChangeList& changes = fake_change_processor_->changes();
  EXPECT_EQ(0u, changes.size());

  // Change current theme to custom theme and notify theme_sync_service_.
  fake_theme_service_->SetTheme(theme_extension_.get());
  theme_sync_service_->OnThemeChanged();
  EXPECT_EQ(1u, changes.size());
  const sync_pb::ThemeSpecifics& change_specifics =
      changes[0].sync_data().GetSpecifics().theme();
  EXPECT_TRUE(change_specifics.use_custom_theme());
  EXPECT_EQ(theme_extension_->id(), change_specifics.custom_theme_id());
  EXPECT_EQ(theme_extension_->name(), change_specifics.custom_theme_name());
  EXPECT_EQ(
      extensions::ManifestURL::GetUpdateURL(theme_extension_.get()).spec(),
      change_specifics.custom_theme_update_url());
}

TEST_F(ThemeSyncableServiceTest, OnThemeChangeByUser_Autogenerated) {
  // Set up theme service to use default theme.
  fake_theme_service_->UseDefaultTheme();

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service_->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(sync_pb::ThemeSpecifics()),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor_.get())));
  EXPECT_FALSE(error.has_value()) << error.value().message();
  const syncer::SyncChangeList& changes = fake_change_processor_->changes();
  EXPECT_EQ(0u, changes.size());

  // Change current theme to custom theme and notify theme_sync_service_.
  fake_theme_service_->BuildAutogeneratedThemeFromColor(
      SkColorSetRGB(0, 0, 100));
  theme_sync_service_->OnThemeChanged();
  EXPECT_EQ(1u, changes.size());
  const sync_pb::ThemeSpecifics& change_specifics =
      changes[0].sync_data().GetSpecifics().theme();
  EXPECT_FALSE(change_specifics.use_custom_theme());
  EXPECT_EQ(fake_theme_service_->GetAutogeneratedThemeColor(),
            SkColorSetRGB(0, 0, 100));
}

TEST_F(ThemeSyncableServiceTest, StopSync) {
  // Set up theme service to use default theme.
  fake_theme_service_->UseDefaultTheme();

  // Start syncing.
  std::optional<syncer::ModelError> merge_error =
      theme_sync_service_->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(sync_pb::ThemeSpecifics()),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor_.get())));
  EXPECT_FALSE(merge_error.has_value()) << merge_error.value().message();
  const syncer::SyncChangeList& changes = fake_change_processor_->changes();
  EXPECT_EQ(0u, changes.size());

  // Stop syncing.
  theme_sync_service_->StopSyncing(syncer::THEMES);

  // Change current theme to custom theme and notify theme_sync_service_.
  // No change is output because sync has stopped.
  fake_theme_service_->SetTheme(theme_extension_.get());
  theme_sync_service_->OnThemeChanged();
  EXPECT_EQ(0u, changes.size());

  // ProcessSyncChanges() should return error when sync has stopped.
  std::optional<syncer::ModelError> process_error =
      theme_sync_service_->ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_TRUE(process_error.has_value());
  EXPECT_EQ("Theme syncable service is not started.",
            process_error.value().message());
}

TEST_F(ThemeSyncableServiceTest, RestoreSystemThemeBitWhenChangeToCustomTheme) {
  // Initialize to use system theme.
  fake_theme_service_->UseDefaultTheme();
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_system_theme_by_default(true);
  std::optional<syncer::ModelError> error =
      theme_sync_service_->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor_.get())));

  // Change to custom theme and notify theme_sync_service_.
  // use_system_theme_by_default bit should be preserved.
  fake_theme_service_->SetTheme(theme_extension_.get());
  theme_sync_service_->OnThemeChanged();
  const syncer::SyncChangeList& changes = fake_change_processor_->changes();
  EXPECT_EQ(1u, changes.size());
  const sync_pb::ThemeSpecifics& change_specifics =
      changes[0].sync_data().GetSpecifics().theme();
  EXPECT_TRUE(change_specifics.use_system_theme_by_default());
}

TEST_F(ThemeSyncableServiceTest, DistinctSystemTheme) {
  fake_theme_service_->set_distinct_from_default_theme(true);

  // Initialize to use native theme.
  fake_theme_service_->UseSystemTheme();
  fake_theme_service_->MarkClean();
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_system_theme_by_default(true);
  std::optional<syncer::ModelError> error =
      theme_sync_service_->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor_.get())));
  EXPECT_FALSE(fake_theme_service_->is_dirty());

  // Change to default theme and notify theme_sync_service_.
  // use_system_theme_by_default bit should be false.
  fake_theme_service_->UseDefaultTheme();
  theme_sync_service_->OnThemeChanged();
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  EXPECT_EQ(1u, changes.size());
  EXPECT_FALSE(changes[0]
                   .sync_data()
                   .GetSpecifics()
                   .theme()
                   .use_system_theme_by_default());

  // Change to native theme and notify theme_sync_service_.
  // use_system_theme_by_default bit should be true.
  changes.clear();
  fake_theme_service_->UseSystemTheme();
  theme_sync_service_->OnThemeChanged();
  EXPECT_EQ(1u, changes.size());
  EXPECT_TRUE(changes[0]
                  .sync_data()
                  .GetSpecifics()
                  .theme()
                  .use_system_theme_by_default());
}

TEST_F(ThemeSyncableServiceTest, SystemThemeSameAsDefaultTheme) {
  fake_theme_service_->set_distinct_from_default_theme(false);

  // Set up theme service to use default theme.
  fake_theme_service_->UseDefaultTheme();

  // Initialize to use custom theme with use_system_theme_by_default set true.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(true);
  theme_specifics.set_custom_theme_id(theme_extension_->id());
  theme_specifics.set_custom_theme_name(kCustomThemeName);
  theme_specifics.set_custom_theme_update_url(kCustomThemeUrl);
  theme_specifics.set_use_system_theme_by_default(true);
  std::optional<syncer::ModelError> error =
      theme_sync_service_->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor_.get())));
  EXPECT_EQ(fake_theme_service_->theme_extension(), theme_extension_.get());

  // Change to default theme and notify theme_sync_service_.
  // use_system_theme_by_default bit should be preserved.
  fake_theme_service_->UseDefaultTheme();
  theme_sync_service_->OnThemeChanged();
  const syncer::SyncChangeList& changes = fake_change_processor_->changes();
  EXPECT_EQ(1u, changes.size());
  const sync_pb::ThemeSpecifics& change_specifics =
      changes[0].sync_data().GetSpecifics().theme();
  EXPECT_FALSE(change_specifics.use_custom_theme());
  EXPECT_TRUE(change_specifics.use_system_theme_by_default());
}

// PolicyInstalledThemeTest ----------------------------------------------------

class PolicyInstalledThemeTest : public ThemeSyncableServiceTest {
  extensions::mojom::ManifestLocation GetThemeLocation() override {
    return extensions::mojom::ManifestLocation::kExternalPolicyDownload;
  }
};

TEST_F(PolicyInstalledThemeTest, InstallThemeByPolicy) {
  // Set up theme service to use custom theme that was installed by policy.
  fake_theme_service_->SetTheme(theme_extension_.get());

  syncer::SyncDataList data_list =
      theme_sync_service_->GetAllSyncDataForTesting(syncer::THEMES);

  ASSERT_EQ(0u, data_list.size());
}

// RealThemeSyncableServiceTest ------------------------------------------------

// Uses the real `ThemeService` impl instead of the fake used by
// `ThemeSyncableServiceTest`, in order to more closely test production behavior
// and asserts.
class RealThemeSyncableServiceTest
    : public extensions::ExtensionServiceTestBase {
 protected:
  void SetUp() override {
    // Setting a matching update URL is necessary to make the test theme
    // considered syncable.
    extension_test_util::SetGalleryUpdateURL(GURL(kCustomThemeUrl));

    // Trying to write the theme pak just produces error messages.
    ThemeService::DisableThemePackForTesting();

    extensions::ExtensionServiceTestBase::SetUp();
    InitializeExtensionService(ExtensionServiceInitParams());
    service_->Init();

    theme_service_ = ThemeServiceFactory::GetForProfile(profile());

    theme_sync_service_ = theme_service_->GetThemeSyncableService();
    ASSERT_TRUE(theme_sync_service_);

    fake_change_processor_ =
        std::make_unique<syncer::FakeSyncChangeProcessor>();

    // Create and add custom theme extension so the ThemeSyncableService can
    // find it.
    theme_extension_ = MakeThemeExtension(
        base::FilePath(kExtensionFilePath), kCustomThemeId, kCustomThemeName,
        extensions::mojom::ManifestLocation::kInternal, kCustomThemeUrl);
    extensions::ExtensionPrefs::Get(profile())->AddGrantedPermissions(
        theme_extension_->id(), extensions::PermissionSet());
    registrar()->AddExtension(theme_extension_);
    ASSERT_EQ(1u, extensions::ExtensionRegistry::Get(profile())
                      ->enabled_extensions()
                      .size());
  }

  ThemeService* theme_service() { return theme_service_; }

  ThemeSyncableService* theme_sync_service() {
    return theme_sync_service_.get();
  }

  syncer::FakeSyncChangeProcessor* fake_change_processor() {
    return fake_change_processor_.get();
  }

  const extensions::Extension* theme_extension() const {
    return theme_extension_.get();
  }

 private:
  raw_ptr<ThemeService> theme_service_;
  raw_ptr<ThemeSyncableService> theme_sync_service_;
  std::unique_ptr<syncer::FakeSyncChangeProcessor> fake_change_processor_;
  scoped_refptr<extensions::Extension> theme_extension_;
};

// Regression test for crbug.com/1409996.
TEST_F(RealThemeSyncableServiceTest, ProcessSyncThemeChange_DisabledExtension) {
  // Set up theme service to use custom theme.
  {
    test::ThemeServiceChangedWaiter waiter(theme_service());
    theme_service()->SetTheme(theme_extension());
    waiter.WaitForThemeChanged();
  }

  // The custom theme should be set and enabled.
  EXPECT_TRUE(theme_service()->UsingExtensionTheme());
  EXPECT_EQ(theme_extension()->id(), theme_service()->GetThemeID());
  EXPECT_TRUE(registrar()->IsExtensionEnabled(theme_extension()->id()));

  // Now disable that theme by changing to an autogenerated theme.
  {
    test::ThemeServiceChangedWaiter waiter(theme_service());
    theme_service()->BuildAutogeneratedThemeFromColor(SkColorSetRGB(0, 0, 100));
    waiter.WaitForThemeChanged();
  }

  // The custom theme should no longer be set or enabled.
  EXPECT_FALSE(theme_service()->UsingExtensionTheme());
  EXPECT_FALSE(registrar()->IsExtensionEnabled(theme_extension()->id()));

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, syncer::SyncDataList(),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  EXPECT_FALSE(error.has_value()) << error.value().message();

  // Process a sync update that updates back to the custom theme.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(true);
  theme_specifics.set_custom_theme_id(theme_extension()->id());
  theme_specifics.set_custom_theme_name(kCustomThemeName);
  theme_specifics.set_custom_theme_update_url(kCustomThemeUrl);
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_theme()->CopyFrom(theme_specifics);
  syncer::SyncChangeList change_list;
  change_list.emplace_back(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      syncer::SyncData::CreateRemoteData(
          entity_specifics, syncer::ClientTagHash::FromHashed("unused")));
  {
    test::ThemeServiceChangedWaiter waiter(theme_service());
    std::optional<syncer::ModelError> process_error =
        theme_sync_service()->ProcessSyncChanges(FROM_HERE, change_list);
    EXPECT_FALSE(process_error.has_value()) << process_error.value().message();
    waiter.WaitForThemeChanged();
  }

  // Ensure the custom theme has been re-set and re-enabled.
  EXPECT_TRUE(theme_service()->UsingExtensionTheme());
  EXPECT_EQ(theme_extension()->id(), theme_service()->GetThemeID());
  EXPECT_TRUE(registrar()->IsExtensionEnabled(theme_extension()->id()));
}

TEST_F(RealThemeSyncableServiceTest,
       UpdateThemeSpecifics_CurrentTheme_Extension) {
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, syncer::SyncDataList(),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor())));
  EXPECT_FALSE(error.has_value()) << error.value().message();

  fake_change_processor()->changes().clear();
  // Set up theme service to use custom theme.
  {
    test::ThemeServiceChangedWaiter waiter(theme_service());
    theme_service()->SetTheme(theme_extension());
    waiter.WaitForThemeChanged();
  }

  const syncer::SyncChangeList& changes = fake_change_processor()->changes();
  ASSERT_EQ(1u, changes.size());
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, changes[0].change_type());
  EXPECT_EQ(syncer::THEMES, changes[0].sync_data().GetDataType());

  const sync_pb::ThemeSpecifics& theme_specifics =
      changes[0].sync_data().GetSpecifics().theme();
  EXPECT_TRUE(theme_specifics.use_custom_theme());
  EXPECT_EQ(theme_extension()->id(), theme_specifics.custom_theme_id());
  EXPECT_EQ(theme_extension()->name(), theme_specifics.custom_theme_name());
  EXPECT_EQ(extensions::ManifestURL::GetUpdateURL(theme_extension()).spec(),
            theme_specifics.custom_theme_update_url());
}

TEST_F(RealThemeSyncableServiceTest,
       UpdateThemeSpecifics_CurrentTheme_Autogenerated) {
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, syncer::SyncDataList(),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor())));
  EXPECT_FALSE(error.has_value()) << error.value().message();

  fake_change_processor()->changes().clear();
  // Set up theme service to use autogenerated theme.
  theme_service()->BuildAutogeneratedThemeFromColor(SkColorSetRGB(0, 0, 100));

  const syncer::SyncChangeList& changes = fake_change_processor()->changes();
  ASSERT_EQ(1u, changes.size());
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, changes[0].change_type());
  EXPECT_EQ(syncer::THEMES, changes[0].sync_data().GetDataType());

  const sync_pb::ThemeSpecifics& theme_specifics =
      changes[0].sync_data().GetSpecifics().theme();
  EXPECT_FALSE(theme_specifics.use_custom_theme());
  EXPECT_EQ(theme_service()->GetAutogeneratedThemeColor(),
            theme_specifics.autogenerated_color_theme().color());
}

TEST_F(RealThemeSyncableServiceTest, UpdateThemeSpecifics_CurrentTheme_Policy) {
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, syncer::SyncDataList(),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor())));
  EXPECT_FALSE(error.has_value()) << error.value().message();

  fake_change_processor()->changes().clear();
  // Set up theme service to use policy theme.
  profile_->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyThemeColor, std::make_unique<base::Value>(100));

  ASSERT_TRUE(theme_service()->UsingPolicyTheme());
  // Applying policy theme doesn't trigger sync changes.
  const syncer::SyncChangeList& changes = fake_change_processor()->changes();
  ASSERT_EQ(0u, changes.size());
}

TEST_F(RealThemeSyncableServiceTest, ShouldDownloadUserColorTheme) {
  sync_pb::ThemeSpecifics theme_specifics;
  sync_pb::ThemeSpecifics::UserColorTheme* user_color_theme =
      theme_specifics.mutable_user_color_theme();
  user_color_theme->set_color(SK_ColorRED);
  user_color_theme->set_browser_color_variant(BrowserColorVariantToProtoEnum(
      ui::mojom::BrowserColorVariant::kTonalSpot));

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  EXPECT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  EXPECT_EQ(theme_service()->GetUserColor(), SK_ColorRED);
  EXPECT_EQ(theme_service()->GetBrowserColorVariant(),
            ui::mojom::BrowserColorVariant::kTonalSpot);
  EXPECT_FALSE(theme_service()->UsingExtensionTheme());
  EXPECT_FALSE(theme_service()->UsingAutogeneratedTheme());
  EXPECT_FALSE(theme_service()->GetIsGrayscale());
  EXPECT_EQ(theme_service()->GetBrowserColorScheme(),
            ThemeService::BrowserColorScheme::kSystem);

  // Verify that the new prefs are used.
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(prefs::kUserColor),
            static_cast<int>(SK_ColorRED));
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(prefs::kBrowserColorVariant),
            static_cast<int>(ui::mojom::BrowserColorVariant::kTonalSpot));
}

TEST_F(RealThemeSyncableServiceTest, ShouldUploadUserColorTheme) {
  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, syncer::SyncDataList(),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  theme_service()->SetUserColorAndBrowserColorVariant(
      SK_ColorRED, ui::mojom::BrowserColorVariant::kTonalSpot);

  const syncer::SyncChangeList& changes = fake_change_processor()->changes();
  ASSERT_GE(changes.size(), 0u);
  const sync_pb::ThemeSpecifics& change_specifics =
      changes.back().sync_data().GetSpecifics().theme();

  EXPECT_FALSE(change_specifics.use_custom_theme());
  ASSERT_TRUE(change_specifics.has_user_color_theme());
  EXPECT_EQ(SK_ColorRED, change_specifics.user_color_theme().color());
  EXPECT_EQ(ui::mojom::BrowserColorVariant::kTonalSpot,
            ProtoEnumToBrowserColorVariant(
                change_specifics.user_color_theme().browser_color_variant()));
  EXPECT_EQ(
      ThemeService::BrowserColorScheme::kSystem,
      ProtoEnumToBrowserColorScheme(change_specifics.browser_color_scheme()));

  // Verify that the old prefs are updated.
  EXPECT_EQ(
      profile()->GetPrefs()->GetInteger(prefs::kDeprecatedUserColorDoNotUse),
      static_cast<int>(SK_ColorRED));
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                prefs::kDeprecatedBrowserColorVariantDoNotUse),
            static_cast<int>(ui::mojom::BrowserColorVariant::kTonalSpot));
}

TEST_F(RealThemeSyncableServiceTest, ShouldDownloadGrayscale) {
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.mutable_grayscale_theme_enabled();

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  EXPECT_TRUE(theme_service()->GetIsGrayscale());
  EXPECT_EQ(theme_service()->GetThemeID(), "");
  EXPECT_EQ(theme_service()->GetUserColor(), std::nullopt);
  EXPECT_EQ(theme_service()->GetBrowserColorVariant(),
            ui::mojom::BrowserColorVariant::kSystem);
  EXPECT_FALSE(theme_service()->UsingExtensionTheme());
  EXPECT_FALSE(theme_service()->UsingAutogeneratedTheme());
  EXPECT_EQ(theme_service()->GetBrowserColorScheme(),
            ThemeService::BrowserColorScheme::kSystem);

  // Verify that the new pref is used.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kGrayscaleThemeEnabled));
}

TEST_F(RealThemeSyncableServiceTest, ShouldUploadGrayscale) {
  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, syncer::SyncDataList(),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  theme_service()->SetIsGrayscale(true);

  const syncer::SyncChangeList& changes = fake_change_processor()->changes();
  ASSERT_GE(changes.size(), 0u);
  const sync_pb::ThemeSpecifics& change_specifics =
      changes.back().sync_data().GetSpecifics().theme();
  EXPECT_FALSE(change_specifics.use_custom_theme());
  EXPECT_TRUE(change_specifics.has_grayscale_theme_enabled());
  EXPECT_EQ(
      ThemeService::BrowserColorScheme::kSystem,
      ProtoEnumToBrowserColorScheme(change_specifics.browser_color_scheme()));

  // Verify that the old pref is updated.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kDeprecatedGrayscaleThemeEnabledDoNotUse));
}

TEST_F(RealThemeSyncableServiceTest, ShouldDownloadBrowserColorScheme) {
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_browser_color_scheme(
      BrowserColorSchemeToProtoEnum(ThemeService::BrowserColorScheme::kLight));

  ASSERT_EQ(theme_service()->GetBrowserColorScheme(),
            ThemeService::BrowserColorScheme::kSystem);

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  EXPECT_EQ(theme_service()->GetBrowserColorScheme(),
            ThemeService::BrowserColorScheme::kLight);
  EXPECT_TRUE(theme_service()->UsingDefaultTheme());
  EXPECT_EQ(theme_service()->GetThemeID(), "");
  EXPECT_EQ(theme_service()->GetUserColor(), std::nullopt);
  EXPECT_EQ(theme_service()->GetBrowserColorVariant(),
            ui::mojom::BrowserColorVariant::kSystem);
  EXPECT_FALSE(theme_service()->GetIsGrayscale());
  EXPECT_FALSE(theme_service()->UsingExtensionTheme());
  EXPECT_FALSE(theme_service()->UsingAutogeneratedTheme());

  // Verify that the new pref is used.
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(prefs::kBrowserColorScheme),
            static_cast<int>(ThemeService::BrowserColorScheme::kLight));
}

TEST_F(RealThemeSyncableServiceTest, ShouldUploadBrowserColorScheme) {
  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, syncer::SyncDataList(),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  theme_service()->SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kLight);

  const syncer::SyncChangeList& changes = fake_change_processor()->changes();
  ASSERT_GE(changes.size(), 0u);
  const sync_pb::ThemeSpecifics& change_specifics =
      changes.back().sync_data().GetSpecifics().theme();

  EXPECT_EQ(
      ThemeService::BrowserColorScheme::kLight,
      ProtoEnumToBrowserColorScheme(change_specifics.browser_color_scheme()));
  EXPECT_FALSE(change_specifics.use_custom_theme());
  EXPECT_FALSE(change_specifics.has_user_color_theme());
  EXPECT_FALSE(change_specifics.has_autogenerated_color_theme());
  EXPECT_FALSE(change_specifics.has_grayscale_theme_enabled());

  // Verify that the old pref is updated.
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                prefs::kDeprecatedBrowserColorSchemeDoNotUse),
            static_cast<int>(ThemeService::BrowserColorScheme::kLight));
}

TEST_F(RealThemeSyncableServiceTest, ShouldDownloadNtpBackground) {
  sync_pb::ThemeSpecifics theme_specifics;
  sync_pb::ThemeSpecifics::NtpCustomBackground* ntp_background =
      theme_specifics.mutable_ntp_background();
  ntp_background->set_url(kTestUrl);
  ntp_background->set_attribution_line_1("attribution_line_1");
  ntp_background->set_attribution_line_2("attribution_line_2");
  ntp_background->set_attribution_action_url("attribution_action_url");
  ntp_background->set_collection_id("collection_id");
  ntp_background->set_resume_token("resume_token");
  ntp_background->set_refresh_timestamp_unix_epoch_seconds(1234567890);
  ntp_background->set_main_color(SK_ColorRED);

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  base::Value::Dict expected_value =
      base::Value::Dict()
          .Set(kNtpCustomBackgroundURL, kTestUrl)
          .Set(kNtpCustomBackgroundAttributionLine1, "attribution_line_1")
          .Set(kNtpCustomBackgroundAttributionLine2, "attribution_line_2")
          .Set(kNtpCustomBackgroundAttributionActionURL,
               "attribution_action_url")
          .Set(kNtpCustomBackgroundCollectionId, "collection_id")
          .Set(kNtpCustomBackgroundResumeToken, "resume_token")
          .Set(kNtpCustomBackgroundRefreshTimestamp,
               static_cast<int>(1234567890))
          .Set(kNtpCustomBackgroundMainColor, static_cast<int>(SK_ColorRED));
  const base::Value* value =
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict);
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, expected_value);
}

TEST_F(RealThemeSyncableServiceTest, ShouldUploadNtpBackground) {
  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, syncer::SyncDataList(),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  base::Value::Dict new_value =
      base::Value::Dict()
          .Set(kNtpCustomBackgroundURL, kTestUrl)
          .Set(kNtpCustomBackgroundAttributionLine1, "attribution_line_1")
          .Set(kNtpCustomBackgroundAttributionLine2, "attribution_line_2")
          .Set(kNtpCustomBackgroundAttributionActionURL,
               "attribution_action_url")
          .Set(kNtpCustomBackgroundCollectionId, "collection_id")
          .Set(kNtpCustomBackgroundResumeToken, "resume_token")
          .Set(kNtpCustomBackgroundRefreshTimestamp,
               static_cast<int>(1234567890))
          .Set(kNtpCustomBackgroundMainColor, static_cast<int>(SK_ColorRED));

  profile()->GetPrefs()->Set(prefs::kNtpCustomBackgroundDict,
                             base::Value(new_value.Clone()));

  const syncer::SyncChangeList& changes = fake_change_processor()->changes();
  ASSERT_GE(changes.size(), 0u);
  const sync_pb::ThemeSpecifics::NtpCustomBackground& ntp_background =
      changes.back().sync_data().GetSpecifics().theme().ntp_background();
  EXPECT_EQ(ntp_background.url(), kTestUrl);
  EXPECT_EQ(ntp_background.attribution_line_1(), "attribution_line_1");
  EXPECT_EQ(ntp_background.attribution_line_2(), "attribution_line_2");
  EXPECT_EQ(ntp_background.attribution_action_url(), "attribution_action_url");
  EXPECT_EQ(ntp_background.collection_id(), "collection_id");
  EXPECT_EQ(ntp_background.resume_token(), "resume_token");
  EXPECT_EQ(ntp_background.refresh_timestamp_unix_epoch_seconds(), 1234567890);
  EXPECT_EQ(ntp_background.main_color(), SK_ColorRED);

  // Verify that the old pref is updated.
  EXPECT_THAT(profile()->GetPrefs()->GetUserPrefValue(
                  prefs::kDeprecatedNtpCustomBackgroundDictDoNotUse),
              DictionaryValuePtrHas(kNtpCustomBackgroundURL, kTestUrl));
}

TEST_F(RealThemeSyncableServiceTest,
       ShouldNotUploadNtpBackgroundIfSetFromLocalResource) {
  base::Value::Dict new_value =
      base::Value::Dict()
          .Set(kNtpCustomBackgroundURL, kTestUrl)
          .Set(kNtpCustomBackgroundAttributionLine1, "attribution_line_1")
          .Set(kNtpCustomBackgroundAttributionLine2, "attribution_line_2")
          .Set(kNtpCustomBackgroundAttributionActionURL,
               "attribution_action_url")
          .Set(kNtpCustomBackgroundCollectionId, "collection_id")
          .Set(kNtpCustomBackgroundResumeToken, "resume_token")
          .Set(kNtpCustomBackgroundRefreshTimestamp,
               static_cast<int>(1234567890))
          .Set(kNtpCustomBackgroundMainColor, static_cast<int>(SK_ColorRED));

  profile()->GetPrefs()->Set(prefs::kNtpCustomBackgroundDict,
                             base::Value(new_value.Clone()));

  // Mark ntp background set from local resource.
  profile()->GetPrefs()->SetBoolean(prefs::kNtpCustomBackgroundLocalToDevice,
                                    true);

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, syncer::SyncDataList(),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  theme_sync_service()->OnThemeChanged();

  const syncer::SyncChangeList& changes = fake_change_processor()->changes();
  ASSERT_GE(changes.size(), 0u);
  EXPECT_FALSE(
      changes.back().sync_data().GetSpecifics().theme().has_ntp_background());

  // Verify that the old pref is not updated.
  EXPECT_FALSE(profile()->GetPrefs()->GetUserPrefValue(
      prefs::kDeprecatedNtpCustomBackgroundDictDoNotUse));
}

TEST_F(RealThemeSyncableServiceTest, ShouldApplyRemoteNtpBackgroundChange) {
  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, syncer::SyncDataList(),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  EXPECT_FALSE(
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict));

  // Process change with background set.
  {
    sync_pb::ThemeSpecifics theme_specifics;
    theme_specifics.mutable_ntp_background()->set_url(kTestUrl);

    ASSERT_FALSE(theme_sync_service()->ProcessSyncChanges(
        FROM_HERE, MakeThemeChangeList(theme_specifics)));
  }

  EXPECT_THAT(
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict),
      DictionaryValuePtrHas(kNtpCustomBackgroundURL, kTestUrl));
}

TEST_F(RealThemeSyncableServiceTest,
       ShouldNotApplyEmptyRemoteNtpBackgroundChange) {
  {
    ScopedDictPrefUpdate dict(profile()->GetPrefs(),
                              prefs::kNtpCustomBackgroundDict);
    dict->Set(kNtpCustomBackgroundURL, kTestUrl);
  }

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, syncer::SyncDataList(),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  EXPECT_THAT(
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict),
      DictionaryValuePtrHas(kNtpCustomBackgroundURL, kTestUrl));

  // Process change with empty background.
  {
    sync_pb::ThemeSpecifics theme_specifics;
    theme_specifics.mutable_ntp_background();
    ASSERT_FALSE(theme_sync_service()->ProcessSyncChanges(
        FROM_HERE, MakeThemeChangeList(theme_specifics)));
  }

  // Removed as the default theme is applied.
  EXPECT_FALSE(
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict));
}

TEST_F(RealThemeSyncableServiceTest,
       ShouldNotApplyMissingRemoteNtpBackgroundChange) {
  {
    ScopedDictPrefUpdate dict(profile()->GetPrefs(),
                              prefs::kNtpCustomBackgroundDict);
    dict->Set(kNtpCustomBackgroundURL, kTestUrl);
  }

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, syncer::SyncDataList(),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  EXPECT_THAT(
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict),
      DictionaryValuePtrHas(kNtpCustomBackgroundURL, kTestUrl));

  // Process change with background not set.
  {
    sync_pb::ThemeSpecifics theme_specifics;
    theme_specifics.set_browser_color_scheme(
        sync_pb::ThemeSpecifics_BrowserColorScheme_SYSTEM);
    ASSERT_FALSE(theme_sync_service()->ProcessSyncChanges(
        FROM_HERE, MakeThemeChangeList(theme_specifics)));
  }

  // Removed as the default theme is applied.
  EXPECT_FALSE(
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict));
}

TEST_F(RealThemeSyncableServiceTest,
       ShouldApplyRemoteBrowserColorSchemeChanges) {
  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, syncer::SyncDataList(),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  EXPECT_EQ(theme_service()->GetBrowserColorScheme(),
            ThemeService::BrowserColorScheme::kSystem);

  // Process change with user color set.
  {
    sync_pb::ThemeSpecifics theme_specifics;
    theme_specifics.set_browser_color_scheme(BrowserColorSchemeToProtoEnum(
        ThemeService::BrowserColorScheme::kLight));

    ASSERT_FALSE(theme_sync_service()->ProcessSyncChanges(
        FROM_HERE, MakeThemeChangeList(theme_specifics)));
  }

  EXPECT_EQ(theme_service()->GetBrowserColorScheme(),
            ThemeService::BrowserColorScheme::kLight);

  // Process change with user color not set.
  {
    sync_pb::ThemeSpecifics theme_specifics;
    // `theme_specifics` has to be non-default to be processed.
    theme_specifics.mutable_autogenerated_color_theme()->set_color(SK_ColorRED);
    ASSERT_FALSE(theme_sync_service()->ProcessSyncChanges(
        FROM_HERE, MakeThemeChangeList(theme_specifics)));
  }

  // No effect.
  EXPECT_EQ(theme_service()->GetBrowserColorScheme(),
            ThemeService::BrowserColorScheme::kLight);

  // Process change with user color set to default.
  {
    sync_pb::ThemeSpecifics theme_specifics;
    theme_specifics.set_browser_color_scheme(BrowserColorSchemeToProtoEnum(
        ThemeService::BrowserColorScheme::kSystem));

    ASSERT_FALSE(theme_sync_service()->ProcessSyncChanges(
        FROM_HERE, MakeThemeChangeList(theme_specifics)));
  }

  // Changes to default.
  EXPECT_EQ(theme_service()->GetBrowserColorScheme(),
            ThemeService::BrowserColorScheme::kSystem);
}

TEST_F(RealThemeSyncableServiceTest,
       ShouldPriortizeExtensionThemeOverUserColor) {
  sync_pb::ThemeSpecifics theme_specifics;
  // Set all fields (all the different theme types).
  theme_specifics.set_use_custom_theme(true);
  theme_specifics.set_custom_theme_id(theme_extension()->id());
  theme_specifics.set_custom_theme_name(kCustomThemeName);
  theme_specifics.set_custom_theme_update_url(kCustomThemeUrl);

  sync_pb::ThemeSpecifics::UserColorTheme* user_color_theme =
      theme_specifics.mutable_user_color_theme();
  user_color_theme->set_color(SK_ColorRED);
  user_color_theme->set_browser_color_variant(BrowserColorVariantToProtoEnum(
      ui::mojom::BrowserColorVariant::kTonalSpot));

  theme_specifics.set_browser_color_scheme(
      BrowserColorSchemeToProtoEnum(ThemeService::BrowserColorScheme::kLight));

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor())));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return theme_service()->UsingExtensionTheme(); }));
  EXPECT_FALSE(theme_service()->UsingAutogeneratedTheme());
  EXPECT_NE(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  EXPECT_EQ(theme_service()->GetUserColor(), std::nullopt);
  EXPECT_NE(theme_service()->GetBrowserColorVariant(),
            ui::mojom::BrowserColorVariant::kTonalSpot);
  EXPECT_FALSE(theme_service()->GetIsGrayscale());
  // Browser color scheme is still applied.
  EXPECT_EQ(theme_service()->GetBrowserColorScheme(),
            ThemeService::BrowserColorScheme::kLight);
}

TEST_F(RealThemeSyncableServiceTest,
       ShouldPriortizeExtensionThemeOverAutogeneratedTheme) {
  sync_pb::ThemeSpecifics theme_specifics;
  // Set all fields (all the different theme types).
  theme_specifics.set_use_custom_theme(true);
  theme_specifics.set_custom_theme_id(theme_extension()->id());
  theme_specifics.set_custom_theme_name(kCustomThemeName);
  theme_specifics.set_custom_theme_update_url(kCustomThemeUrl);
  theme_specifics.mutable_autogenerated_color_theme()->set_color(SK_ColorBLUE);
  theme_specifics.set_browser_color_scheme(
      BrowserColorSchemeToProtoEnum(ThemeService::BrowserColorScheme::kLight));

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor())));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return theme_service()->UsingExtensionTheme(); }));
  EXPECT_FALSE(theme_service()->UsingAutogeneratedTheme());
  EXPECT_NE(theme_service()->GetThemeID(), ThemeService::kAutogeneratedThemeID);
  EXPECT_EQ(theme_service()->GetUserColor(), std::nullopt);
  EXPECT_FALSE(theme_service()->GetIsGrayscale());
  // Browser color scheme is still applied.
  EXPECT_EQ(theme_service()->GetBrowserColorScheme(),
            ThemeService::BrowserColorScheme::kLight);
}

TEST_F(RealThemeSyncableServiceTest,
       ShouldPriortizeExtensionThemeOverGrayscale) {
  sync_pb::ThemeSpecifics theme_specifics;
  // Set all fields (all the different theme types).
  theme_specifics.set_use_custom_theme(true);
  theme_specifics.set_custom_theme_id(theme_extension()->id());
  theme_specifics.set_custom_theme_name(kCustomThemeName);
  theme_specifics.set_custom_theme_update_url(kCustomThemeUrl);
  theme_specifics.mutable_grayscale_theme_enabled();
  theme_specifics.set_browser_color_scheme(
      BrowserColorSchemeToProtoEnum(ThemeService::BrowserColorScheme::kLight));

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor())));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return theme_service()->UsingExtensionTheme(); }));
  EXPECT_FALSE(theme_service()->GetIsGrayscale());
  EXPECT_FALSE(theme_service()->UsingAutogeneratedTheme());
  EXPECT_EQ(theme_service()->GetUserColor(), std::nullopt);
  EXPECT_EQ(theme_service()->GetBrowserColorVariant(),
            ui::mojom::BrowserColorVariant::kSystem);
  // Browser color scheme is still applied.
  EXPECT_EQ(theme_service()->GetBrowserColorScheme(),
            ThemeService::BrowserColorScheme::kLight);
}

TEST_F(RealThemeSyncableServiceTest,
       ShouldPrioritizeExtensionThemeInAreThemeSpecificsEquivalent) {
  sync_pb::ThemeSpecifics a, b;
  a.set_use_custom_theme(true);
  a.set_browser_color_scheme(
      BrowserColorSchemeToProtoEnum(ThemeService::BrowserColorScheme::kLight));
  a.mutable_autogenerated_color_theme()->set_color(SK_ColorBLUE);
  a.set_use_system_theme_by_default(true);

  b.set_use_custom_theme(true);
  b.set_browser_color_scheme(
      BrowserColorSchemeToProtoEnum(ThemeService::BrowserColorScheme::kDark));
  b.mutable_grayscale_theme_enabled();
  b.set_use_system_theme_by_default(false);

  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, true));
}

TEST_F(RealThemeSyncableServiceTest,
       ShouldConsiderBrowserColorSchemeInAreThemeSpecificsEquivalent) {
  sync_pb::ThemeSpecifics a, b;
  a.set_use_custom_theme(false);
  a.set_browser_color_scheme(
      BrowserColorSchemeToProtoEnum(ThemeService::BrowserColorScheme::kLight));
  a.mutable_autogenerated_color_theme()->set_color(SK_ColorBLUE);
  a.set_use_system_theme_by_default(true);

  b = a;
  b.mutable_autogenerated_color_theme()->set_color(SK_ColorRED);
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));

  b.mutable_autogenerated_color_theme()->set_color(SK_ColorRED);
  b.set_browser_color_scheme(
      BrowserColorSchemeToProtoEnum(ThemeService::BrowserColorScheme::kDark));
  // AreThemeSpecificsEquivalent returns false even though only the browser
  // color scheme differs.
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));
}

TEST_F(RealThemeSyncableServiceTest,
       ShouldConsiderNtpBackgroundInAreThemeSpecificsEquivalent) {
  sync_pb::ThemeSpecifics a, b;
  a.set_use_custom_theme(false);
  a.mutable_autogenerated_color_theme()->set_color(SK_ColorBLUE);
  a.mutable_ntp_background()->set_url(kTestUrl);

  b = a;

  sync_pb::ThemeSpecifics::NtpCustomBackground* background =
      b.mutable_ntp_background();

  EXPECT_TRUE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));

  // Set a different url.
  background->set_url("https://www.bar.com");
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));

  // Set a different collection id.
  background->set_url(kTestUrl);
  background->set_collection_id("collection_id");
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));

  // Set a different main color.
  background->clear_collection_id();
  background->set_main_color(SK_ColorRED);
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));

  // Set a different refresh timestamp.
  background->clear_main_color();
  background->set_refresh_timestamp_unix_epoch_seconds(1234567890);
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));

  // Set a different resume token.
  background->clear_refresh_timestamp_unix_epoch_seconds();
  background->set_resume_token("resume_token");
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));

  // Set a different attribution action url.
  background->clear_resume_token();
  background->set_attribution_action_url(kTestUrl);
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));

  // Set a different attribution line 1.
  background->clear_attribution_action_url();
  background->set_attribution_line_1("attribution_line_1");
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));

  // Set a different attribution line 2.
  background->clear_attribution_line_1();
  background->set_attribution_line_2("attribution_line_2");
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));

  background->clear_attribution_line_2();
  ASSERT_TRUE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));

  // Remove the ntp background.
  b.clear_ntp_background();
  EXPECT_FALSE(ThemeSyncableService::AreThemeSpecificsEquivalent(a, b, false));
}

TEST_F(RealThemeSyncableServiceTest,
       ShouldApplyBrowserColorSchemeWithUserColorTheme) {
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(false);
  theme_specifics.set_browser_color_scheme(
      BrowserColorSchemeToProtoEnum(ThemeService::BrowserColorScheme::kLight));
  sync_pb::ThemeSpecifics::UserColorTheme* user_color_theme =
      theme_specifics.mutable_user_color_theme();
  user_color_theme->set_color(SK_ColorRED);
  user_color_theme->set_browser_color_variant(BrowserColorVariantToProtoEnum(
      ui::mojom::BrowserColorVariant::kTonalSpot));

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor())));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  EXPECT_FALSE(theme_service()->UsingExtensionTheme());
  EXPECT_FALSE(theme_service()->UsingAutogeneratedTheme());
  EXPECT_FALSE(theme_service()->GetIsGrayscale());
  EXPECT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  EXPECT_EQ(theme_service()->GetUserColor(), SK_ColorRED);
  EXPECT_EQ(theme_service()->GetBrowserColorVariant(),
            ui::mojom::BrowserColorVariant::kTonalSpot);
  EXPECT_EQ(theme_service()->GetBrowserColorScheme(),
            ThemeService::BrowserColorScheme::kLight);
}

TEST_F(RealThemeSyncableServiceTest,
       ShouldApplyBrowserColorSchemeWithGrayscale) {
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(false);
  theme_specifics.mutable_grayscale_theme_enabled();
  theme_specifics.set_browser_color_scheme(
      BrowserColorSchemeToProtoEnum(ThemeService::BrowserColorScheme::kLight));

  // Start syncing.
  test::ThemeServiceChangedWaiter waiter(theme_service());
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor())));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  EXPECT_FALSE(theme_service()->UsingExtensionTheme());
  EXPECT_FALSE(theme_service()->UsingAutogeneratedTheme());
  EXPECT_EQ(theme_service()->GetThemeID(), "");
  EXPECT_EQ(theme_service()->GetUserColor(), std::nullopt);
  EXPECT_EQ(theme_service()->GetBrowserColorVariant(),
            ui::mojom::BrowserColorVariant::kSystem);
  EXPECT_TRUE(theme_service()->GetIsGrayscale());
  EXPECT_EQ(theme_service()->GetBrowserColorScheme(),
            ThemeService::BrowserColorScheme::kLight);
}

TEST_F(RealThemeSyncableServiceTest,
       ShouldApplyBrowserColorSchemeWithAutogeneratedTheme) {
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(false);
  theme_specifics.mutable_autogenerated_color_theme()->set_color(SK_ColorBLUE);
  theme_specifics.set_browser_color_scheme(
      BrowserColorSchemeToProtoEnum(ThemeService::BrowserColorScheme::kLight));

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor())));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  EXPECT_FALSE(theme_service()->UsingExtensionTheme());
  EXPECT_TRUE(theme_service()->UsingAutogeneratedTheme());
  EXPECT_EQ(theme_service()->GetAutogeneratedThemeColor(), SK_ColorBLUE);
  EXPECT_EQ(theme_service()->GetThemeID(), ThemeService::kAutogeneratedThemeID);
  EXPECT_EQ(theme_service()->GetUserColor(), std::nullopt);
  EXPECT_EQ(theme_service()->GetBrowserColorVariant(),
            ui::mojom::BrowserColorVariant::kSystem);
  EXPECT_FALSE(theme_service()->GetIsGrayscale());
  EXPECT_EQ(theme_service()->GetBrowserColorScheme(),
            ThemeService::BrowserColorScheme::kLight);
}

TEST_F(RealThemeSyncableServiceTest,
       ShouldApplyNtpBackgroundWithUserColorTheme) {
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(false);
  theme_specifics.mutable_ntp_background()->set_url(kTestUrl);
  sync_pb::ThemeSpecifics::UserColorTheme* user_color_theme =
      theme_specifics.mutable_user_color_theme();
  user_color_theme->set_color(SK_ColorRED);
  user_color_theme->set_browser_color_variant(BrowserColorVariantToProtoEnum(
      ui::mojom::BrowserColorVariant::kTonalSpot));

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor())));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  EXPECT_FALSE(theme_service()->UsingExtensionTheme());
  EXPECT_FALSE(theme_service()->UsingAutogeneratedTheme());
  EXPECT_FALSE(theme_service()->GetIsGrayscale());
  EXPECT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  EXPECT_EQ(theme_service()->GetUserColor(), SK_ColorRED);
  EXPECT_EQ(theme_service()->GetBrowserColorVariant(),
            ui::mojom::BrowserColorVariant::kTonalSpot);
  EXPECT_THAT(
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict),
      DictionaryValuePtrHas(kNtpCustomBackgroundURL, kTestUrl));
}

TEST_F(RealThemeSyncableServiceTest, ShouldApplyNtpBackgroundWithGrayscale) {
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(false);
  theme_specifics.mutable_grayscale_theme_enabled();
  theme_specifics.mutable_ntp_background()->set_url(kTestUrl);

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor())));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  EXPECT_FALSE(theme_service()->UsingExtensionTheme());
  EXPECT_FALSE(theme_service()->UsingAutogeneratedTheme());
  EXPECT_EQ(theme_service()->GetThemeID(), "");
  EXPECT_EQ(theme_service()->GetUserColor(), std::nullopt);
  EXPECT_EQ(theme_service()->GetBrowserColorVariant(),
            ui::mojom::BrowserColorVariant::kSystem);
  EXPECT_TRUE(theme_service()->GetIsGrayscale());
  EXPECT_THAT(
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict),
      DictionaryValuePtrHas(kNtpCustomBackgroundURL, kTestUrl));
}

TEST_F(RealThemeSyncableServiceTest,
       ShouldApplyNtpBackgroundWithAutogeneratedTheme) {
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(false);
  theme_specifics.mutable_autogenerated_color_theme()->set_color(SK_ColorBLUE);
  theme_specifics.mutable_ntp_background()->set_url(kTestUrl);

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor())));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  EXPECT_FALSE(theme_service()->UsingExtensionTheme());
  EXPECT_TRUE(theme_service()->UsingAutogeneratedTheme());
  EXPECT_EQ(theme_service()->GetAutogeneratedThemeColor(), SK_ColorBLUE);
  EXPECT_EQ(theme_service()->GetThemeID(), ThemeService::kAutogeneratedThemeID);
  EXPECT_EQ(theme_service()->GetUserColor(), std::nullopt);
  EXPECT_EQ(theme_service()->GetBrowserColorVariant(),
            ui::mojom::BrowserColorVariant::kSystem);
  EXPECT_FALSE(theme_service()->GetIsGrayscale());
  EXPECT_THAT(
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict),
      DictionaryValuePtrHas(kNtpCustomBackgroundURL, kTestUrl));
}

TEST_F(RealThemeSyncableServiceTest,
       ShouldNotApplyDefaultThemeFromOldSpecificsUponMergeDataAndStartSyncing) {
  theme_service()->SetUserColorAndBrowserColorVariant(
      SK_ColorRED, ui::mojom::BrowserColorVariant::kTonalSpot);
  ASSERT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  ASSERT_EQ(theme_service()->GetUserColor(), SK_ColorRED);

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(sync_pb::ThemeSpecifics()),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor())));

  EXPECT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  EXPECT_EQ(theme_service()->GetUserColor(), SK_ColorRED);
}

TEST_F(RealThemeSyncableServiceTest,
       ShouldNotApplyDefaultThemeFromOldSpecificsUponProcessSyncChanges) {
  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, syncer::SyncDataList(),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  theme_service()->SetUserColorAndBrowserColorVariant(
      SK_ColorRED, ui::mojom::BrowserColorVariant::kTonalSpot);
  ASSERT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  ASSERT_EQ(theme_service()->GetUserColor(), SK_ColorRED);

  ASSERT_FALSE(theme_sync_service()->ProcessSyncChanges(
      FROM_HERE, MakeThemeChangeList(sync_pb::ThemeSpecifics())));

  EXPECT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  EXPECT_EQ(theme_service()->GetUserColor(), SK_ColorRED);
}

TEST_F(RealThemeSyncableServiceTest,
       ShouldApplyDefaultThemeFromNewSpecificsUponProcessSyncChanges) {
  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, syncer::SyncDataList(),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  theme_service()->SetUserColorAndBrowserColorVariant(
      SK_ColorRED, ui::mojom::BrowserColorVariant::kTonalSpot);
  ASSERT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  ASSERT_EQ(theme_service()->GetUserColor(), SK_ColorRED);

  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_browser_color_scheme(
      sync_pb::ThemeSpecifics_BrowserColorScheme_SYSTEM);
  ASSERT_FALSE(theme_sync_service()->ProcessSyncChanges(
      FROM_HERE, MakeThemeChangeList(theme_specifics)));

  EXPECT_TRUE(theme_service()->UsingDefaultTheme());
  EXPECT_NE(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  EXPECT_EQ(theme_service()->GetUserColor(), std::nullopt);
}

TEST_F(RealThemeSyncableServiceTest,
       ShouldApplyNonDefaultThemeFromOldSpecificsUponProcessSyncChanges) {
  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, syncer::SyncDataList(),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  theme_service()->SetUserColorAndBrowserColorVariant(
      SK_ColorRED, ui::mojom::BrowserColorVariant::kTonalSpot);
  ASSERT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  ASSERT_EQ(theme_service()->GetUserColor(), SK_ColorRED);

  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.mutable_autogenerated_color_theme()->set_color(SK_ColorRED);
  ASSERT_FALSE(theme_sync_service()->ProcessSyncChanges(
      FROM_HERE, MakeThemeChangeList(theme_specifics)));

  EXPECT_TRUE(theme_service()->UsingAutogeneratedTheme());
  EXPECT_NE(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  EXPECT_EQ(theme_service()->GetUserColor(), std::nullopt);
}

TEST_F(RealThemeSyncableServiceTest, ShouldUpdateOldSyncingThemePrefs) {
  // Start syncing.
  ASSERT_FALSE(theme_sync_service()->MergeDataAndStartSyncing(
      syncer::THEMES, syncer::SyncDataList(),
      std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
          fake_change_processor())));

  ASSERT_FALSE(profile()->GetPrefs()->GetUserPrefValue(
      prefs::kDeprecatedUserColorDoNotUse));
  ASSERT_FALSE(profile()->GetPrefs()->GetUserPrefValue(
      prefs::kDeprecatedBrowserColorVariantDoNotUse));
  ASSERT_FALSE(profile()->GetPrefs()->GetUserPrefValue(
      prefs::kDeprecatedGrayscaleThemeEnabledDoNotUse));
  ASSERT_FALSE(
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict));

  // Set user color theme.
  theme_service()->SetUserColorAndBrowserColorVariant(
      SK_ColorRED, ui::mojom::BrowserColorVariant::kTonalSpot);

  ASSERT_TRUE(profile()->GetPrefs()->GetUserPrefValue(
      prefs::kDeprecatedUserColorDoNotUse));
  ASSERT_TRUE(profile()->GetPrefs()->GetUserPrefValue(
      prefs::kDeprecatedBrowserColorVariantDoNotUse));
  EXPECT_EQ(
      profile()->GetPrefs()->GetInteger(prefs::kDeprecatedUserColorDoNotUse),
      static_cast<int>(SK_ColorRED));
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                prefs::kDeprecatedBrowserColorVariantDoNotUse),
            static_cast<int>(ui::mojom::BrowserColorVariant::kTonalSpot));

  // Other prefs are cleared.
  EXPECT_FALSE(profile()->GetPrefs()->GetUserPrefValue(
      prefs::kDeprecatedGrayscaleThemeEnabledDoNotUse));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict));

  // Set grayscale theme.
  theme_service()->SetIsGrayscale(true);

  ASSERT_TRUE(profile()->GetPrefs()->GetUserPrefValue(
      prefs::kDeprecatedGrayscaleThemeEnabledDoNotUse));
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kDeprecatedGrayscaleThemeEnabledDoNotUse));

  // Other prefs are cleared.
  EXPECT_FALSE(profile()->GetPrefs()->GetUserPrefValue(
      prefs::kDeprecatedUserColorDoNotUse));
  EXPECT_FALSE(profile()->GetPrefs()->GetUserPrefValue(
      prefs::kDeprecatedBrowserColorVariantDoNotUse));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict));

  // Set ntp background.
  {
    ScopedDictPrefUpdate dict(profile()->GetPrefs(),
                              prefs::kNtpCustomBackgroundDict);
    dict->Set(kNtpCustomBackgroundURL, kTestUrl);
  }

  EXPECT_TRUE(
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict));

  // Other prefs are left as-is.
  EXPECT_TRUE(profile()->GetPrefs()->GetUserPrefValue(
      prefs::kDeprecatedGrayscaleThemeEnabledDoNotUse));
  EXPECT_FALSE(profile()->GetPrefs()->GetUserPrefValue(
      prefs::kDeprecatedUserColorDoNotUse));
  EXPECT_FALSE(profile()->GetPrefs()->GetUserPrefValue(
      prefs::kDeprecatedBrowserColorVariantDoNotUse));

  // Set default theme.
  theme_service()->UseDefaultTheme();

  // All prefs are cleared.
  EXPECT_FALSE(profile()->GetPrefs()->GetUserPrefValue(
      prefs::kDeprecatedUserColorDoNotUse));
  EXPECT_FALSE(profile()->GetPrefs()->GetUserPrefValue(
      prefs::kDeprecatedBrowserColorVariantDoNotUse));
  EXPECT_FALSE(profile()->GetPrefs()->GetUserPrefValue(
      prefs::kDeprecatedGrayscaleThemeEnabledDoNotUse));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict));
}

// Regression test for crbug.com/389026436.
TEST_F(RealThemeSyncableServiceTest, ClearLocalNtpBackgroundIfRemoteEmpty) {
  // Set local ntp background.
  base::Value::Dict new_value =
      base::Value::Dict()
          .Set(kNtpCustomBackgroundURL, kTestUrl)
          .Set(kNtpCustomBackgroundAttributionLine1, "attribution_line_1")
          .Set(kNtpCustomBackgroundAttributionLine2, "attribution_line_2")
          .Set(kNtpCustomBackgroundAttributionActionURL,
               "attribution_action_url")
          .Set(kNtpCustomBackgroundCollectionId, "collection_id")
          .Set(kNtpCustomBackgroundResumeToken, "resume_token")
          .Set(kNtpCustomBackgroundRefreshTimestamp, 1234567890)
          .Set(kNtpCustomBackgroundMainColor, static_cast<int>(SK_ColorRED));

  profile()->GetPrefs()->Set(prefs::kNtpCustomBackgroundDict,
                             base::Value(new_value.Clone()));

  // Remote theme.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(false);
  theme_specifics.set_browser_color_scheme(
      ::sync_pb::ThemeSpecifics_BrowserColorScheme_SYSTEM);
  sync_pb::ThemeSpecifics::UserColorTheme* user_color_theme =
      theme_specifics.mutable_user_color_theme();
  user_color_theme->set_color(SK_ColorRED);
  user_color_theme->set_browser_color_variant(
      sync_pb::ThemeSpecifics_UserColorTheme_BrowserColorVariant_TONAL_SPOT);

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  // Local ntp background is cleared.
  EXPECT_FALSE(
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict));
}

// Regression test for crbug.com/391114025.
TEST_F(RealThemeSyncableServiceTest,
       KeepLocalNtpBackgroundUponNonDefaultOldThemeSpecifics) {
  // Set local ntp background.
  base::Value::Dict new_value =
      base::Value::Dict()
          .Set(kNtpCustomBackgroundURL, kTestUrl)
          .Set(kNtpCustomBackgroundAttributionLine1, "attribution_line_1")
          .Set(kNtpCustomBackgroundAttributionLine2, "attribution_line_2")
          .Set(kNtpCustomBackgroundAttributionActionURL,
               "attribution_action_url")
          .Set(kNtpCustomBackgroundCollectionId, "collection_id")
          .Set(kNtpCustomBackgroundResumeToken, "resume_token")
          .Set(kNtpCustomBackgroundRefreshTimestamp, 1234567890)
          .Set(kNtpCustomBackgroundMainColor, static_cast<int>(SK_ColorRED));

  profile()->GetPrefs()->Set(prefs::kNtpCustomBackgroundDict,
                             base::Value(new_value.Clone()));

  // Remote theme does not contain new fields, thus an old ThemeSpecifics.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(false);
  theme_specifics.mutable_autogenerated_color_theme()->set_color(SK_ColorBLUE);

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  // Local ntp background is still there. The remote theme was produced by an
  // old client which didn't know about the new ThemeSpecifics fields. It didn't
  // intentionally clear the background, just left it unset.
  EXPECT_TRUE(theme_service()->UsingAutogeneratedTheme());
  EXPECT_EQ(theme_service()->GetAutogeneratedThemeColor(), SK_ColorBLUE);
  EXPECT_TRUE(
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict));
  EXPECT_EQ(profile()->GetPrefs()->GetDict(prefs::kNtpCustomBackgroundDict),
            new_value);

  // The merged theme should be committed to the server.
  const syncer::SyncChangeList& changes = fake_change_processor()->changes();
  ASSERT_EQ(changes.size(), 1u);
  const sync_pb::ThemeSpecifics& change_specifics =
      changes.back().sync_data().GetSpecifics().theme();
  ASSERT_TRUE(change_specifics.has_browser_color_scheme());
  ASSERT_TRUE(change_specifics.has_autogenerated_color_theme());
  EXPECT_EQ(change_specifics.autogenerated_color_theme().color(), SK_ColorBLUE);
  ASSERT_TRUE(change_specifics.has_ntp_background());
  EXPECT_EQ(change_specifics.ntp_background().url(), kTestUrl);
}

// Regression test for crbug.com/389026436.
TEST_F(RealThemeSyncableServiceTest,
       KeepLocalNtpBackgroundUponDefaultOldThemeSpecifics) {
  // Set local ntp background.
  base::Value::Dict new_value =
      base::Value::Dict()
          .Set(kNtpCustomBackgroundURL, kTestUrl)
          .Set(kNtpCustomBackgroundAttributionLine1, "attribution_line_1")
          .Set(kNtpCustomBackgroundAttributionLine2, "attribution_line_2")
          .Set(kNtpCustomBackgroundAttributionActionURL,
               "attribution_action_url")
          .Set(kNtpCustomBackgroundCollectionId, "collection_id")
          .Set(kNtpCustomBackgroundResumeToken, "resume_token")
          .Set(kNtpCustomBackgroundRefreshTimestamp, 1234567890)
          .Set(kNtpCustomBackgroundMainColor, static_cast<int>(SK_ColorRED));

  profile()->GetPrefs()->Set(prefs::kNtpCustomBackgroundDict,
                             base::Value(new_value.Clone()));

  // Remote theme does not contain new fields, thus an old ThemeSpecifics.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(false);

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  // Local ntp background is still there since default remote themes are ignored
  // in the initial update.
  EXPECT_TRUE(
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict));
  EXPECT_EQ(profile()->GetPrefs()->GetDict(prefs::kNtpCustomBackgroundDict),
            new_value);

  sync_pb::ThemeSpecifics current_specifics =
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting();
  ASSERT_TRUE(current_specifics.has_browser_color_scheme());
  ASSERT_TRUE(current_specifics.has_ntp_background());
  EXPECT_EQ(current_specifics.ntp_background().url(), kTestUrl);
}

// Regression test for crbug.com/389026436.
TEST_F(RealThemeSyncableServiceTest,
       ClearLocalNtpBackgroundUponNonDefaultNewThemeSpecifics) {
  // Set local ntp background.
  base::Value::Dict new_value =
      base::Value::Dict()
          .Set(kNtpCustomBackgroundURL, kTestUrl)
          .Set(kNtpCustomBackgroundAttributionLine1, "attribution_line_1")
          .Set(kNtpCustomBackgroundAttributionLine2, "attribution_line_2")
          .Set(kNtpCustomBackgroundAttributionActionURL,
               "attribution_action_url")
          .Set(kNtpCustomBackgroundCollectionId, "collection_id")
          .Set(kNtpCustomBackgroundResumeToken, "resume_token")
          .Set(kNtpCustomBackgroundRefreshTimestamp, 1234567890)
          .Set(kNtpCustomBackgroundMainColor, static_cast<int>(SK_ColorRED));

  profile()->GetPrefs()->Set(prefs::kNtpCustomBackgroundDict,
                             base::Value(new_value.Clone()));

  // Remote theme contains new fields, thus a new ThemeSpecifics.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(false);
  theme_specifics.set_browser_color_scheme(
      ::sync_pb::ThemeSpecifics_BrowserColorScheme_SYSTEM);
  theme_specifics.mutable_autogenerated_color_theme()->set_color(SK_ColorBLUE);

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  // Local ntp background is cleared, because the remote client must have
  // explicitly cleared it.
  EXPECT_TRUE(theme_service()->UsingAutogeneratedTheme());
  EXPECT_FALSE(
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict));

  // The remote theme wins and nothing is committed to the server.
  ASSERT_EQ(fake_change_processor()->changes().size(), 0u);
}

TEST_F(RealThemeSyncableServiceTest,
       KeepLocalNtpBackgroundUponDefaultNewThemeSpecifics) {
  // Set local ntp background.
  base::Value::Dict new_value =
      base::Value::Dict()
          .Set(kNtpCustomBackgroundURL, kTestUrl)
          .Set(kNtpCustomBackgroundAttributionLine1, "attribution_line_1")
          .Set(kNtpCustomBackgroundAttributionLine2, "attribution_line_2")
          .Set(kNtpCustomBackgroundAttributionActionURL,
               "attribution_action_url")
          .Set(kNtpCustomBackgroundCollectionId, "collection_id")
          .Set(kNtpCustomBackgroundResumeToken, "resume_token")
          .Set(kNtpCustomBackgroundRefreshTimestamp, 1234567890)
          .Set(kNtpCustomBackgroundMainColor, static_cast<int>(SK_ColorRED));

  profile()->GetPrefs()->Set(prefs::kNtpCustomBackgroundDict,
                             base::Value(new_value.Clone()));

  // Remote theme contains new fields, thus a new ThemeSpecifics.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(false);
  theme_specifics.set_browser_color_scheme(
      ::sync_pb::ThemeSpecifics_BrowserColorScheme_SYSTEM);

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  // Local ntp background is still there since default remote themes are ignored
  // in the initial update.
  EXPECT_TRUE(
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict));
  EXPECT_EQ(profile()->GetPrefs()->GetDict(prefs::kNtpCustomBackgroundDict),
            new_value);

  sync_pb::ThemeSpecifics current_specifics =
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting();
  ASSERT_TRUE(current_specifics.has_browser_color_scheme());
  ASSERT_TRUE(current_specifics.has_ntp_background());
  EXPECT_EQ(current_specifics.ntp_background().url(), kTestUrl);
}

TEST_F(RealThemeSyncableServiceTest,
       ClearLocalUserColorUponNonDefaultOldThemeSpecifics) {
  // Set local user color.
  theme_service()->SetUserColorAndBrowserColorVariant(
      SK_ColorBLUE, ui::mojom::BrowserColorVariant::kNeutral);
  ASSERT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  ASSERT_EQ(theme_service()->GetUserColor(), SK_ColorBLUE);

  // Remote theme does not contain new fields, thus an old ThemeSpecifics.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(false);
  theme_specifics.mutable_autogenerated_color_theme()->set_color(SK_ColorBLUE);

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  // Local user color is cleared because user color and autogenerated color
  // cannot co-exist.
  EXPECT_TRUE(theme_service()->UsingAutogeneratedTheme());
  EXPECT_NE(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  EXPECT_FALSE(theme_service()->GetUserColor());

  // The remote theme wins and nothing is committed to the server.
  ASSERT_EQ(fake_change_processor()->changes().size(), 0u);
}

TEST_F(RealThemeSyncableServiceTest,
       KeepLocalUserColorUponDefaultOldThemeSpecifics) {
  // Set local user color.
  theme_service()->SetUserColorAndBrowserColorVariant(
      SK_ColorBLUE, ui::mojom::BrowserColorVariant::kNeutral);
  ASSERT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  ASSERT_EQ(theme_service()->GetUserColor(), SK_ColorBLUE);

  // Remote theme does not contain new fields, thus an old ThemeSpecifics.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(false);

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  // Local user color is still there since default remote themes are ignored in
  // the initial update.
  EXPECT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  EXPECT_EQ(theme_service()->GetUserColor(), SK_ColorBLUE);

  sync_pb::ThemeSpecifics current_specifics =
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting();
  ASSERT_TRUE(current_specifics.has_browser_color_scheme());
  ASSERT_TRUE(current_specifics.has_user_color_theme());
  EXPECT_EQ(current_specifics.user_color_theme().color(), SK_ColorBLUE);
  EXPECT_EQ(current_specifics.user_color_theme().browser_color_variant(),
            sync_pb::ThemeSpecifics_UserColorTheme_BrowserColorVariant_NEUTRAL);
}

TEST_F(RealThemeSyncableServiceTest,
       ClearLocalUserColorUponNonDefaultNewThemeSpecifics) {
  // Set local user color.
  theme_service()->SetUserColorAndBrowserColorVariant(
      SK_ColorBLUE, ui::mojom::BrowserColorVariant::kNeutral);
  ASSERT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  ASSERT_EQ(theme_service()->GetUserColor(), SK_ColorBLUE);

  // Remote theme contains new fields, thus a new ThemeSpecifics.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(false);
  theme_specifics.set_browser_color_scheme(
      ::sync_pb::ThemeSpecifics_BrowserColorScheme_SYSTEM);
  theme_specifics.mutable_ntp_background()->set_url(kTestUrl);
  ASSERT_FALSE(
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict));

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  EXPECT_TRUE(
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict));
  // Local user color is cleared since the remote client must have explicitly
  // cleared it.
  EXPECT_NE(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  EXPECT_FALSE(theme_service()->GetUserColor());

  // The remote theme wins and nothing is committed to the server.
  ASSERT_EQ(fake_change_processor()->changes().size(), 0u);
}

TEST_F(RealThemeSyncableServiceTest,
       KeepLocalUserColorUponDefaultNewThemeSpecifics) {
  // Set local user color.
  theme_service()->SetUserColorAndBrowserColorVariant(
      SK_ColorBLUE, ui::mojom::BrowserColorVariant::kNeutral);
  ASSERT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  ASSERT_EQ(theme_service()->GetUserColor(), SK_ColorBLUE);

  // Remote theme contains new fields, thus a new ThemeSpecifics.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(false);
  theme_specifics.set_browser_color_scheme(
      ::sync_pb::ThemeSpecifics_BrowserColorScheme_SYSTEM);

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  // Local user color is still there since default remote themes are ignored in
  // the initial update.
  EXPECT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  EXPECT_EQ(theme_service()->GetUserColor(), SK_ColorBLUE);

  sync_pb::ThemeSpecifics current_specifics =
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting();
  ASSERT_TRUE(current_specifics.has_browser_color_scheme());
  ASSERT_TRUE(current_specifics.has_user_color_theme());
  EXPECT_EQ(current_specifics.user_color_theme().color(), SK_ColorBLUE);
  EXPECT_EQ(current_specifics.user_color_theme().browser_color_variant(),
            sync_pb::ThemeSpecifics_UserColorTheme_BrowserColorVariant_NEUTRAL);
}

TEST_F(RealThemeSyncableServiceTest,
       ShouldNotCommitIfLocalAndRemoteThemeAreSame) {
  // Set local user color.
  theme_service()->SetUserColorAndBrowserColorVariant(
      SK_ColorBLUE, ui::mojom::BrowserColorVariant::kNeutral);

  // Remote theme same as local theme.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(false);
  theme_specifics.set_use_system_theme_by_default(false);
  theme_specifics.set_browser_color_scheme(
      ::sync_pb::ThemeSpecifics_BrowserColorScheme_SYSTEM);
  theme_specifics.mutable_user_color_theme()->set_color(SK_ColorBLUE);
  theme_specifics.mutable_user_color_theme()->set_browser_color_variant(
      ::sync_pb::ThemeSpecifics_UserColorTheme_BrowserColorVariant_NEUTRAL);

  ASSERT_EQ(theme_specifics.SerializeAsString(),
            theme_sync_service()
                ->GetThemeSpecificsFromCurrentThemeForTesting()
                .SerializeAsString());

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  // Nothing is committed.
  ASSERT_EQ(fake_change_processor()->changes().size(), 0u);
}

TEST_F(RealThemeSyncableServiceTest,
       ShouldNotCommitAnythingElseWithExtensionTheme) {
  // Local extension theme.
  {
    test::ThemeServiceChangedWaiter waiter(theme_service());
    theme_service()->SetTheme(theme_extension());
    waiter.WaitForThemeChanged();
  }
  ASSERT_TRUE(theme_service()->UsingExtensionTheme());

  theme_sync_service()->MergeDataAndStartSyncing(
      syncer::THEMES, MakeThemeDataList(sync_pb::ThemeSpecifics()),
      std::unique_ptr<syncer::SyncChangeProcessor>(
          new syncer::SyncChangeProcessorWrapperForTest(
              fake_change_processor())));

  sync_pb::ThemeSpecifics expected_theme_specifics;
  expected_theme_specifics.set_use_custom_theme(true);
  expected_theme_specifics.set_browser_color_scheme(
      expected_theme_specifics.SYSTEM);
  expected_theme_specifics.set_use_system_theme_by_default(false);
  expected_theme_specifics.set_custom_theme_name(kCustomThemeName);
  expected_theme_specifics.set_custom_theme_id(kCustomThemeId);
  expected_theme_specifics.set_custom_theme_update_url(kCustomThemeUrl);
  EXPECT_THAT(
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
      base::test::EqualsProto(expected_theme_specifics));

  // Set custom ntp background pref.
  {
    ScopedDictPrefUpdate dict(profile()->GetPrefs(),
                              prefs::kNtpCustomBackgroundDict);
    dict->Set(kNtpCustomBackgroundURL, kTestUrl);
  }
  task_environment()->RunUntilIdle();
  // Local extension theme is still there.
  ASSERT_TRUE(theme_service()->UsingExtensionTheme());

  // ThemeSpecifics should be valid, i.e. should not contain ntp background when
  // there is an extension theme.
  EXPECT_THAT(
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
      base::test::EqualsProto(expected_theme_specifics));
  // Only the extension theme is committed.
  ASSERT_GE(fake_change_processor()->changes().size(), 1u);
  EXPECT_THAT(fake_change_processor()
                  ->changes()
                  .back()
                  .sync_data()
                  .GetSpecifics()
                  .theme(),
              base::test::EqualsProto(expected_theme_specifics));
}

TEST_F(RealThemeSyncableServiceTest,
       ShouldApplyBrowserColorSchemeAlongsideExtensionTheme) {
  theme_service()->SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kLight);

  // Set remote extension theme with dark browser color scheme.
  sync_pb::ThemeSpecifics theme_specifics =
      theme_service::test::CreateThemeSpecificsWithExtensionTheme(
          kCustomThemeId, kCustomThemeName, kCustomThemeUrl);
  theme_specifics.set_browser_color_scheme(
      ::sync_pb::ThemeSpecifics_BrowserColorScheme_DARK);

  ASSERT_FALSE(theme_sync_service()
                   ->MergeDataAndStartSyncing(
                       syncer::THEMES, MakeThemeDataList(theme_specifics),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               fake_change_processor())))
                   .has_value());

  // Remote extension theme with dark browser color scheme is applied.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return theme_service()->UsingExtensionTheme(); }));
  EXPECT_EQ(theme_service()->GetBrowserColorScheme(),
            ThemeService::BrowserColorScheme::kDark);
}

TEST_F(RealThemeSyncableServiceTest,
       ShouldCommitBrowserColorSchemeAlongsideNewExtensionTheme) {
  // Local browser color scheme.
  theme_service()->SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kLight);
  // Start syncing.
  ASSERT_FALSE(theme_sync_service()
                   ->MergeDataAndStartSyncing(
                       syncer::THEMES, syncer::SyncDataList(),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               fake_change_processor())))
                   .has_value());

  // Set a new extension theme.
  theme_service()->SetTheme(theme_extension());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return theme_service()->UsingExtensionTheme(); }));

  // Extension theme should be applied and committed alongside the browser color
  // scheme.
  ASSERT_EQ(theme_service()->GetBrowserColorScheme(),
            ThemeService::BrowserColorScheme::kLight);
  ASSERT_THAT(fake_change_processor()->changes(), Not(testing::IsEmpty()));
  EXPECT_THAT(
      fake_change_processor()
          ->changes()
          .back()
          .sync_data()
          .GetSpecifics()
          .theme(),
      testing::AllOf(
          testing::Property(&sync_pb::ThemeSpecifics::use_custom_theme, true),
          testing::Property(&sync_pb::ThemeSpecifics::browser_color_scheme,
                            sync_pb::ThemeSpecifics::LIGHT)));
}

TEST_F(RealThemeSyncableServiceTest,
       ShouldCommitBrowserColorSchemeIfPreexistingExtensionTheme) {
  // Local extension theme.
  theme_service()->SetTheme(theme_extension());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return theme_service()->UsingExtensionTheme(); }));
  // Start syncing.
  ASSERT_FALSE(theme_sync_service()
                   ->MergeDataAndStartSyncing(
                       syncer::THEMES, syncer::SyncDataList(),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               fake_change_processor())))
                   .has_value());

  // Set a browser color scheme. This should be applied and committed alongside
  // the extension theme.
  theme_service()->SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kLight);
  ASSERT_TRUE(theme_service()->UsingExtensionTheme());

  ASSERT_THAT(fake_change_processor()->changes(), Not(testing::IsEmpty()));
  EXPECT_THAT(
      fake_change_processor()
          ->changes()
          .back()
          .sync_data()
          .GetSpecifics()
          .theme(),
      testing::AllOf(
          testing::Property(&sync_pb::ThemeSpecifics::use_custom_theme, true),
          testing::Property(&sync_pb::ThemeSpecifics::browser_color_scheme,
                            sync_pb::ThemeSpecifics::LIGHT)));
}

class ThemeSyncableServiceVerifyFinalStateTest
    : public RealThemeSyncableServiceTest,
      public testing::WithParamInterface<sync_pb::ThemeSpecifics> {
 protected:
  void MergeRemoteUpdateAndVerify() {
    sync_pb::ThemeSpecifics theme_specifics = GetParam();

    // Skip test if remote theme is the same.
    if (theme_sync_service()
            ->GetThemeSpecificsFromCurrentThemeForTesting()
            .SerializeAsString() == theme_specifics.SerializeAsString()) {
      return;
    }

    // Start syncing.
    std::optional<syncer::ModelError> error =
        theme_sync_service()->MergeDataAndStartSyncing(
            syncer::THEMES, MakeThemeDataList(theme_specifics),
            std::unique_ptr<syncer::SyncChangeProcessor>(
                new syncer::SyncChangeProcessorWrapperForTest(
                    fake_change_processor())));
    ASSERT_FALSE(error.has_value()) << error.value().message();

    if (theme_specifics.use_custom_theme()) {
      // Remote extension theme is not applied instantaneously.
      EXPECT_TRUE(base::test::RunUntil(
          [&]() { return theme_service()->UsingExtensionTheme(); }));
      // Remote extension theme produces more than one commits.
      EXPECT_GE(fake_change_processor()->changes().size(), 1u);
    } else {
      EXPECT_THAT(fake_change_processor()->changes(), testing::IsEmpty());
    }

    // Current theme matches the remote theme.
    EXPECT_THAT(
        theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
        base::test::EqualsProto(theme_specifics));
  }
};

TEST_P(ThemeSyncableServiceVerifyFinalStateTest, LocalDefaultTheme) {
  MergeRemoteUpdateAndVerify();
}

TEST_P(ThemeSyncableServiceVerifyFinalStateTest, LocalExtensionTheme) {
  // Local extension theme.
  {
    test::ThemeServiceChangedWaiter waiter(theme_service());
    theme_service()->SetTheme(theme_extension());
    waiter.WaitForThemeChanged();
  }
  ASSERT_TRUE(theme_service()->UsingExtensionTheme());

  MergeRemoteUpdateAndVerify();
}

TEST_P(ThemeSyncableServiceVerifyFinalStateTest, LocalAutogeneratedColorTheme) {
  // Local autogenerated color theme.
  theme_service()->BuildAutogeneratedThemeFromColor(SK_ColorBLUE);
  ASSERT_TRUE(theme_service()->UsingAutogeneratedTheme());

  MergeRemoteUpdateAndVerify();
}

TEST_P(ThemeSyncableServiceVerifyFinalStateTest, LocalColorTheme) {
  // Local color theme.
  theme_service()->SetUserColorAndBrowserColorVariant(
      SK_ColorBLUE, ui::mojom::BrowserColorVariant::kTonalSpot);
  ASSERT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);

  MergeRemoteUpdateAndVerify();
}

TEST_P(ThemeSyncableServiceVerifyFinalStateTest, LocalGrayscaleTheme) {
  // Local grayscale theme.
  theme_service()->SetIsGrayscale(true);
  ASSERT_TRUE(theme_service()->GetIsGrayscale());

  MergeRemoteUpdateAndVerify();
}

TEST_P(ThemeSyncableServiceVerifyFinalStateTest, LocalBackground) {
  // Local custom ntp background.
  {
    ScopedDictPrefUpdate dict(profile()->GetPrefs(),
                              prefs::kNtpCustomBackgroundDict);
    dict->Set(kNtpCustomBackgroundURL, kTestUrl);
  }

  MergeRemoteUpdateAndVerify();
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ThemeSyncableServiceVerifyFinalStateTest,
    testing::Values(
        theme_service::test::CreateThemeSpecificsWithExtensionTheme(
            kCustomThemeId,
            kCustomThemeName,
            kCustomThemeUrl),
        theme_service::test::CreateThemeSpecificsWithAutogeneratedColorTheme(),
        theme_service::test::CreateThemeSpecificsWithColorTheme(),
        theme_service::test::CreateThemeSpecificsWithGrayscaleTheme(),
        theme_service::test::CreateThemeSpecificsWithCustomNtpBackground(
            kTestUrl)));

class ThemeSyncableServiceTestWithoutAccountThemesSeparation
    : public RealThemeSyncableServiceTest {
 public:
  ThemeSyncableServiceTestWithoutAccountThemesSeparation() {
    feature_list_.InitAndDisableFeature(syncer::kSeparateLocalAndAccountThemes);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ThemeSyncableServiceTestWithoutAccountThemesSeparation,
       LocalThemeIsCommitedUponInitialSync) {
  theme_service()->BuildAutogeneratedThemeFromColor(SK_ColorBLUE);
  ASSERT_TRUE(theme_service()->UsingAutogeneratedTheme());

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(sync_pb::ThemeSpecifics()),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor())));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  // One pending change.
  EXPECT_EQ(fake_change_processor()->changes().size(), 1u);
}

TEST_F(ThemeSyncableServiceTestWithoutAccountThemesSeparation,
       ShouldCommitExtensionThemeAndBrowserColorSchemeOnInitialSync) {
  // Local extension theme with browser color scheme.
  theme_service()->SetTheme(theme_extension());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return theme_service()->UsingExtensionTheme(); }));
  theme_service()->SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kLight);

  // Start syncing.
  ASSERT_FALSE(theme_sync_service()
                   ->MergeDataAndStartSyncing(
                       syncer::THEMES, syncer::SyncDataList(),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               fake_change_processor())))
                   .has_value());

  EXPECT_TRUE(theme_service()->UsingExtensionTheme());
  EXPECT_EQ(theme_service()->GetBrowserColorScheme(),
            ThemeService::BrowserColorScheme::kLight);
  EXPECT_THAT(
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
      testing::AllOf(
          testing::Property(&sync_pb::ThemeSpecifics::use_custom_theme, true),
          testing::Property(&sync_pb::ThemeSpecifics::browser_color_scheme,
                            sync_pb::ThemeSpecifics::LIGHT)));
  // Local extension theme is committed along with the browser color scheme.
  ASSERT_EQ(fake_change_processor()->changes().size(), 1u);
  EXPECT_THAT(
      fake_change_processor()->changes()[0].sync_data().GetSpecifics().theme(),
      base::test::EqualsProto(
          theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting()));
}

class ThemeSyncableServiceTestWithAccountThemesSeparation
    : public RealThemeSyncableServiceTest {
 public:
  ThemeSyncableServiceTestWithAccountThemesSeparation()
      : feature_list_(syncer::kSeparateLocalAndAccountThemes) {}

  sync_pb::ThemeSpecifics ReadSavedLocalThemeSpecifics() {
    std::string encoded_str =
        profile_->GetPrefs()->GetString(prefs::kSavedLocalTheme);
    std::string decoded_str;
    EXPECT_TRUE(base::Base64Decode(encoded_str, &decoded_str));

    sync_pb::ThemeSpecifics specifics;
    EXPECT_TRUE(specifics.ParseFromString(decoded_str));
    return specifics;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ThemeSyncableServiceTestWithAccountThemesSeparation,
       LocalThemeIsNotCommitedUponInitialSync) {
  theme_service()->BuildAutogeneratedThemeFromColor(SK_ColorBLUE);
  ASSERT_TRUE(theme_service()->UsingAutogeneratedTheme());

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(sync_pb::ThemeSpecifics()),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor())));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  // No pending change.
  EXPECT_THAT(fake_change_processor()->changes(), ::testing::IsEmpty());
}

TEST_F(ThemeSyncableServiceTestWithAccountThemesSeparation,
       SaveLocalExtensionThemeToPrefUponInitialSync) {
  // Set up theme service to use custom theme.
  {
    test::ThemeServiceChangedWaiter waiter(theme_service());
    theme_service()->SetTheme(theme_extension());
    waiter.WaitForThemeChanged();
  }
  ASSERT_TRUE(theme_service()->UsingExtensionTheme());

  // Start initial sync.
  theme_sync_service()->WillStartInitialSync();

  // Check the saved local theme.
  sync_pb::ThemeSpecifics local_theme_specifics =
      ReadSavedLocalThemeSpecifics();
  EXPECT_TRUE(local_theme_specifics.use_custom_theme());
  EXPECT_EQ(local_theme_specifics.custom_theme_name(), kCustomThemeName);
  EXPECT_EQ(local_theme_specifics.custom_theme_id(), theme_extension()->id());
  EXPECT_EQ(local_theme_specifics.custom_theme_update_url(), kCustomThemeUrl);
}

TEST_F(ThemeSyncableServiceTestWithAccountThemesSeparation,
       SaveLocalAutogeneratedThemeToPrefUponInitialSync) {
  theme_service()->BuildAutogeneratedThemeFromColor(SK_ColorBLUE);
  ASSERT_TRUE(theme_service()->UsingAutogeneratedTheme());

  // Start initial sync.
  theme_sync_service()->WillStartInitialSync();

  // Check the saved local theme.
  sync_pb::ThemeSpecifics local_theme_specifics =
      ReadSavedLocalThemeSpecifics();
  EXPECT_FALSE(local_theme_specifics.use_custom_theme());
  EXPECT_EQ(local_theme_specifics.browser_color_scheme(),
            sync_pb::ThemeSpecifics_BrowserColorScheme_SYSTEM);
  ASSERT_TRUE(local_theme_specifics.has_autogenerated_color_theme());
  EXPECT_EQ(local_theme_specifics.autogenerated_color_theme().color(),
            SK_ColorBLUE);
}

TEST_F(ThemeSyncableServiceTestWithAccountThemesSeparation,
       SaveLocalUserColorThemeToPrefUponInitialSync) {
  theme_service()->SetUserColorAndBrowserColorVariant(
      SK_ColorRED, ui::mojom::BrowserColorVariant::kTonalSpot);
  ASSERT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);

  // Start initial sync.
  theme_sync_service()->WillStartInitialSync();

  // Check the saved local theme.
  sync_pb::ThemeSpecifics local_theme_specifics =
      ReadSavedLocalThemeSpecifics();
  EXPECT_FALSE(local_theme_specifics.use_custom_theme());
  EXPECT_EQ(local_theme_specifics.browser_color_scheme(),
            sync_pb::ThemeSpecifics_BrowserColorScheme_SYSTEM);
  ASSERT_TRUE(local_theme_specifics.has_user_color_theme());
  EXPECT_EQ(local_theme_specifics.user_color_theme().color(), SK_ColorRED);
  EXPECT_EQ(
      local_theme_specifics.user_color_theme().browser_color_variant(),
      sync_pb::ThemeSpecifics_UserColorTheme_BrowserColorVariant_TONAL_SPOT);
}

TEST_F(ThemeSyncableServiceTestWithAccountThemesSeparation,
       SaveLocalGrayscaleThemeToPrefUponInitialSync) {
  theme_service()->SetIsGrayscale(true);

  // Start initial sync.
  theme_sync_service()->WillStartInitialSync();

  // Check the saved local theme.
  sync_pb::ThemeSpecifics local_theme_specifics =
      ReadSavedLocalThemeSpecifics();
  EXPECT_FALSE(local_theme_specifics.use_custom_theme());
  EXPECT_EQ(local_theme_specifics.browser_color_scheme(),
            sync_pb::ThemeSpecifics_BrowserColorScheme_SYSTEM);
  EXPECT_TRUE(local_theme_specifics.has_grayscale_theme_enabled());
}

TEST_F(ThemeSyncableServiceTestWithAccountThemesSeparation,
       SaveLocalUseSystemThemeToPrefUponInitialSync) {
  if (!theme_service()->IsSystemThemeDistinctFromDefaultTheme()) {
    return;
  }
  theme_service()->UseSystemTheme();

  // Start initial sync.
  theme_sync_service()->WillStartInitialSync();

  // Check the saved local theme.
  sync_pb::ThemeSpecifics local_theme_specifics =
      ReadSavedLocalThemeSpecifics();
  EXPECT_FALSE(local_theme_specifics.use_custom_theme());
  EXPECT_EQ(local_theme_specifics.browser_color_scheme(),
            sync_pb::ThemeSpecifics_BrowserColorScheme_SYSTEM);
  ASSERT_TRUE(local_theme_specifics.has_use_system_theme_by_default());
  EXPECT_TRUE(local_theme_specifics.use_system_theme_by_default());
}

TEST_F(ThemeSyncableServiceTestWithAccountThemesSeparation,
       SaveLocalBrowserColorSchemeToPrefUponInitialSync) {
  theme_service()->SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kLight);

  // Start initial sync.
  theme_sync_service()->WillStartInitialSync();

  // Check the saved local theme.
  sync_pb::ThemeSpecifics local_theme_specifics =
      ReadSavedLocalThemeSpecifics();
  EXPECT_FALSE(local_theme_specifics.use_custom_theme());
  EXPECT_EQ(local_theme_specifics.browser_color_scheme(),
            sync_pb::ThemeSpecifics_BrowserColorScheme_LIGHT);
}

TEST_F(ThemeSyncableServiceTestWithAccountThemesSeparation,
       SaveLocalNtpBackgroundToPrefUponInitialSync) {
  // Set custom background via pref.
  base::Value::Dict background_dict =
      base::Value::Dict()
          .Set(kNtpCustomBackgroundURL, kTestUrl)
          .Set(kNtpCustomBackgroundAttributionLine1, "attribution_line_1")
          .Set(kNtpCustomBackgroundAttributionLine2, "attribution_line_2")
          .Set(kNtpCustomBackgroundAttributionActionURL,
               "attribution_action_url")
          .Set(kNtpCustomBackgroundCollectionId, "collection_id")
          .Set(kNtpCustomBackgroundResumeToken, "resume_token")
          .Set(kNtpCustomBackgroundRefreshTimestamp,
               static_cast<int>(1234567890))
          .Set(kNtpCustomBackgroundMainColor, static_cast<int>(SK_ColorRED));

  profile()->GetPrefs()->Set(prefs::kNtpCustomBackgroundDict,
                             base::Value(background_dict.Clone()));

  // Start initial sync.
  theme_sync_service()->WillStartInitialSync();

  // Check the saved local theme.
  sync_pb::ThemeSpecifics local_theme_specifics =
      ReadSavedLocalThemeSpecifics();
  EXPECT_FALSE(local_theme_specifics.use_custom_theme());
  EXPECT_EQ(local_theme_specifics.browser_color_scheme(),
            sync_pb::ThemeSpecifics_BrowserColorScheme_SYSTEM);
  ASSERT_TRUE(local_theme_specifics.has_ntp_background());
  EXPECT_EQ(local_theme_specifics.ntp_background().url(), kTestUrl);
}

TEST_F(ThemeSyncableServiceTestWithAccountThemesSeparation,
       ShouldNotSaveLocalThemeToPrefUponBrowserRestart) {
  theme_service()->SetUserColorAndBrowserColorVariant(
      SK_ColorRED, ui::mojom::BrowserColorVariant::kTonalSpot);
  ASSERT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(sync_pb::ThemeSpecifics()),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor())));

  // No theme was saved.
  EXPECT_FALSE(profile_->GetPrefs()->GetUserPrefValue(prefs::kSavedLocalTheme));
}

TEST_F(ThemeSyncableServiceTestWithAccountThemesSeparation,
       LoadsLocalExtensionThemeFromPrefUponSyncStop) {
  // Set remote extension theme.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(false);
  theme_specifics.mutable_autogenerated_color_theme()->set_color(SK_ColorBLUE);
  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor())));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  EXPECT_TRUE(theme_service()->UsingAutogeneratedTheme());
  EXPECT_FALSE(theme_service()->UsingExtensionTheme());

  // Set saved local theme pref.
  sync_pb::ThemeSpecifics local_theme_specifics;
  local_theme_specifics.set_use_custom_theme(true);
  local_theme_specifics.set_custom_theme_id(theme_extension()->id());
  local_theme_specifics.set_custom_theme_name(kCustomThemeName);
  local_theme_specifics.set_custom_theme_update_url(kCustomThemeUrl);

  profile()->GetPrefs()->SetString(
      prefs::kSavedLocalTheme,
      base::Base64Encode(local_theme_specifics.SerializeAsString()));

  base::HistogramTester histogram_tester;
  // Stop syncing.
  {
    test::ThemeServiceChangedWaiter waiter(theme_service());
    theme_sync_service()->StopSyncing(syncer::THEMES);
    waiter.WaitForThemeChanged();
  }
  EXPECT_FALSE(theme_service()->UsingAutogeneratedTheme());
  EXPECT_TRUE(theme_service()->UsingExtensionTheme());
  EXPECT_EQ(theme_service()->GetThemeID(), theme_extension()->id());
  histogram_tester.ExpectUniqueSample("Theme.RestoredLocalThemeUponSignout",
                                      true, 1);
}

TEST_F(ThemeSyncableServiceTestWithAccountThemesSeparation,
       LoadsLocalAutogeneratedColorThemeFromPrefUponSyncStop) {
  // Set remote extension theme.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(true);
  theme_specifics.set_custom_theme_id(theme_extension()->id());
  theme_specifics.set_custom_theme_name(kCustomThemeName);
  theme_specifics.set_custom_theme_update_url(kCustomThemeUrl);

  // Start syncing.
  {
    test::ThemeServiceChangedWaiter waiter(theme_service());
    std::optional<syncer::ModelError> error =
        theme_sync_service()->MergeDataAndStartSyncing(
            syncer::THEMES, MakeThemeDataList(theme_specifics),
            std::unique_ptr<syncer::SyncChangeProcessor>(
                new syncer::SyncChangeProcessorWrapperForTest(
                    fake_change_processor())));
    ASSERT_FALSE(error.has_value()) << error.value().message();
    waiter.WaitForThemeChanged();
  }
  EXPECT_TRUE(theme_service()->UsingExtensionTheme());
  EXPECT_FALSE(theme_service()->UsingAutogeneratedTheme());

  // Set saved local theme pref.
  sync_pb::ThemeSpecifics local_theme_specifics;
  local_theme_specifics.set_use_custom_theme(false);
  local_theme_specifics.mutable_autogenerated_color_theme()->set_color(
      SK_ColorBLUE);

  profile()->GetPrefs()->SetString(
      prefs::kSavedLocalTheme,
      base::Base64Encode(local_theme_specifics.SerializeAsString()));

  base::HistogramTester histogram_tester;
  // Stop syncing.
  {
    test::ThemeServiceChangedWaiter waiter(theme_service());
    theme_sync_service()->StopSyncing(syncer::THEMES);
    waiter.WaitForThemeChanged();
  }
  EXPECT_FALSE(theme_service()->UsingExtensionTheme());
  EXPECT_TRUE(theme_service()->UsingAutogeneratedTheme());
  EXPECT_EQ(theme_service()->GetAutogeneratedThemeColor(), SK_ColorBLUE);
  histogram_tester.ExpectUniqueSample("Theme.RestoredLocalThemeUponSignout",
                                      true, 1);
}

TEST_F(ThemeSyncableServiceTestWithAccountThemesSeparation,
       LoadsLocalUserColorThemeFromPrefUponSyncStop) {
  // Set remote extension theme.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(false);
  theme_specifics.mutable_autogenerated_color_theme()->set_color(SK_ColorBLUE);

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor())));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  EXPECT_TRUE(theme_service()->UsingAutogeneratedTheme());
  EXPECT_NE(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);

  // Set saved local theme pref.
  sync_pb::ThemeSpecifics local_theme_specifics;
  local_theme_specifics.set_use_custom_theme(false);
  sync_pb::ThemeSpecifics::UserColorTheme* user_color_theme =
      local_theme_specifics.mutable_user_color_theme();
  user_color_theme->set_color(SK_ColorRED);
  user_color_theme->set_browser_color_variant(
      sync_pb::ThemeSpecifics_UserColorTheme_BrowserColorVariant_TONAL_SPOT);

  profile()->GetPrefs()->SetString(
      prefs::kSavedLocalTheme,
      base::Base64Encode(local_theme_specifics.SerializeAsString()));

  base::HistogramTester histogram_tester;
  // Stop syncing.
  theme_sync_service()->StopSyncing(syncer::THEMES);

  // Theme is restored to user color theme.
  EXPECT_FALSE(theme_service()->UsingAutogeneratedTheme());
  EXPECT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  EXPECT_EQ(theme_service()->GetUserColor(), static_cast<int>(SK_ColorRED));
  EXPECT_EQ(theme_service()->GetBrowserColorVariant(),
            ui::mojom::BrowserColorVariant::kTonalSpot);
  histogram_tester.ExpectUniqueSample("Theme.RestoredLocalThemeUponSignout",
                                      true, 1);
}

TEST_F(ThemeSyncableServiceTestWithAccountThemesSeparation,
       LoadsLocalGrayscaleThemeFromPrefUponSyncStop) {
  // Set remote extension theme.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(true);
  theme_specifics.set_custom_theme_id(theme_extension()->id());
  theme_specifics.set_custom_theme_name(kCustomThemeName);
  theme_specifics.set_custom_theme_update_url(kCustomThemeUrl);

  // Start syncing.
  {
    test::ThemeServiceChangedWaiter waiter(theme_service());
    std::optional<syncer::ModelError> error =
        theme_sync_service()->MergeDataAndStartSyncing(
            syncer::THEMES, MakeThemeDataList(theme_specifics),
            std::unique_ptr<syncer::SyncChangeProcessor>(
                new syncer::SyncChangeProcessorWrapperForTest(
                    fake_change_processor())));
    ASSERT_FALSE(error.has_value()) << error.value().message();
    waiter.WaitForThemeChanged();
  }
  EXPECT_TRUE(theme_service()->UsingExtensionTheme());
  EXPECT_FALSE(theme_service()->GetIsGrayscale());

  // Set saved local theme pref.
  sync_pb::ThemeSpecifics local_theme_specifics;
  local_theme_specifics.set_use_custom_theme(false);
  local_theme_specifics.mutable_grayscale_theme_enabled();

  profile()->GetPrefs()->SetString(
      prefs::kSavedLocalTheme,
      base::Base64Encode(local_theme_specifics.SerializeAsString()));

  base::HistogramTester histogram_tester;
  // Stop syncing.
  {
    test::ThemeServiceChangedWaiter waiter(theme_service());
    theme_sync_service()->StopSyncing(syncer::THEMES);
    waiter.WaitForThemeChanged();
  }
  EXPECT_FALSE(theme_service()->UsingExtensionTheme());
  EXPECT_TRUE(theme_service()->GetIsGrayscale());
  histogram_tester.ExpectUniqueSample("Theme.RestoredLocalThemeUponSignout",
                                      true, 1);
}

TEST_F(ThemeSyncableServiceTestWithAccountThemesSeparation,
       LoadsLocalUseSystemThemeFromPrefUponSyncStop) {
  if (!theme_service()->IsSystemThemeDistinctFromDefaultTheme()) {
    return;
  }

  // Set remote extension theme.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(true);
  theme_specifics.set_custom_theme_id(theme_extension()->id());
  theme_specifics.set_custom_theme_name(kCustomThemeName);
  theme_specifics.set_custom_theme_update_url(kCustomThemeUrl);

  // Start syncing.
  {
    test::ThemeServiceChangedWaiter waiter(theme_service());
    std::optional<syncer::ModelError> error =
        theme_sync_service()->MergeDataAndStartSyncing(
            syncer::THEMES, MakeThemeDataList(theme_specifics),
            std::unique_ptr<syncer::SyncChangeProcessor>(
                new syncer::SyncChangeProcessorWrapperForTest(
                    fake_change_processor())));
    ASSERT_FALSE(error.has_value()) << error.value().message();
    waiter.WaitForThemeChanged();
  }
  EXPECT_TRUE(theme_service()->UsingExtensionTheme());
  EXPECT_FALSE(theme_service()->UsingSystemTheme());

  // Set saved local theme pref.
  sync_pb::ThemeSpecifics local_theme_specifics;
  local_theme_specifics.set_use_custom_theme(false);
  local_theme_specifics.set_browser_color_scheme(
      sync_pb::ThemeSpecifics_BrowserColorScheme_SYSTEM);
  local_theme_specifics.set_use_system_theme_by_default(true);

  profile()->GetPrefs()->SetString(
      prefs::kSavedLocalTheme,
      base::Base64Encode(local_theme_specifics.SerializeAsString()));

  base::HistogramTester histogram_tester;
  // Stop syncing.
  {
    test::ThemeServiceChangedWaiter waiter(theme_service());
    theme_sync_service()->StopSyncing(syncer::THEMES);
    waiter.WaitForThemeChanged();
  }
  EXPECT_FALSE(theme_service()->UsingExtensionTheme());
  EXPECT_TRUE(theme_service()->UsingSystemTheme());
  histogram_tester.ExpectUniqueSample("Theme.RestoredLocalThemeUponSignout",
                                      true, 1);
}

TEST_F(ThemeSyncableServiceTestWithAccountThemesSeparation,
       LoadsLocalBrowserColorSchemeFromPrefUponSyncStop) {
  // Set remote extension theme.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(true);
  theme_specifics.set_custom_theme_id(theme_extension()->id());
  theme_specifics.set_custom_theme_name(kCustomThemeName);
  theme_specifics.set_custom_theme_update_url(kCustomThemeUrl);

  // Start syncing.
  {
    test::ThemeServiceChangedWaiter waiter(theme_service());
    std::optional<syncer::ModelError> error =
        theme_sync_service()->MergeDataAndStartSyncing(
            syncer::THEMES, MakeThemeDataList(theme_specifics),
            std::unique_ptr<syncer::SyncChangeProcessor>(
                new syncer::SyncChangeProcessorWrapperForTest(
                    fake_change_processor())));
    ASSERT_FALSE(error.has_value()) << error.value().message();
    waiter.WaitForThemeChanged();
  }
  EXPECT_TRUE(theme_service()->UsingExtensionTheme());
  EXPECT_EQ(theme_service()->GetBrowserColorScheme(),
            ThemeService::BrowserColorScheme::kSystem);

  // Set saved local theme pref.
  sync_pb::ThemeSpecifics local_theme_specifics;
  local_theme_specifics.set_use_custom_theme(false);
  local_theme_specifics.set_browser_color_scheme(
      sync_pb::ThemeSpecifics_BrowserColorScheme_LIGHT);

  profile()->GetPrefs()->SetString(
      prefs::kSavedLocalTheme,
      base::Base64Encode(local_theme_specifics.SerializeAsString()));

  base::HistogramTester histogram_tester;
  // Stop syncing.
  {
    test::ThemeServiceChangedWaiter waiter(theme_service());
    theme_sync_service()->StopSyncing(syncer::THEMES);
    waiter.WaitForThemeChanged();
  }
  EXPECT_FALSE(theme_service()->UsingExtensionTheme());
  EXPECT_EQ(theme_service()->GetBrowserColorScheme(),
            ThemeService::BrowserColorScheme::kLight);
  histogram_tester.ExpectUniqueSample("Theme.RestoredLocalThemeUponSignout",
                                      true, 1);
}

TEST_F(ThemeSyncableServiceTestWithAccountThemesSeparation,
       LoadsLocalNtpBackgroundFromPrefUponSyncStop) {
  // Set remote extension theme.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(true);
  theme_specifics.set_custom_theme_id(theme_extension()->id());
  theme_specifics.set_custom_theme_name(kCustomThemeName);
  theme_specifics.set_custom_theme_update_url(kCustomThemeUrl);

  // Start syncing.
  {
    test::ThemeServiceChangedWaiter waiter(theme_service());
    std::optional<syncer::ModelError> error =
        theme_sync_service()->MergeDataAndStartSyncing(
            syncer::THEMES, MakeThemeDataList(theme_specifics),
            std::unique_ptr<syncer::SyncChangeProcessor>(
                new syncer::SyncChangeProcessorWrapperForTest(
                    fake_change_processor())));
    ASSERT_FALSE(error.has_value()) << error.value().message();
    waiter.WaitForThemeChanged();
  }
  EXPECT_TRUE(theme_service()->UsingExtensionTheme());
  EXPECT_FALSE(
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict));

  // Set saved local theme pref.
  sync_pb::ThemeSpecifics local_theme_specifics;
  local_theme_specifics.set_use_custom_theme(false);
  sync_pb::ThemeSpecifics::NtpCustomBackground* background =
      local_theme_specifics.mutable_ntp_background();
  background->set_url(kTestUrl);
  background->set_attribution_line_1("attribution_line_1");
  background->set_attribution_line_2("attribution_line_2");
  background->set_attribution_action_url("attribution_action_url");
  background->set_collection_id("collection_id");
  background->set_resume_token("resume_token");
  background->set_refresh_timestamp_unix_epoch_seconds(1234567890);
  background->set_main_color(static_cast<int>(SK_ColorRED));

  profile()->GetPrefs()->SetString(
      prefs::kSavedLocalTheme,
      base::Base64Encode(local_theme_specifics.SerializeAsString()));

  base::HistogramTester histogram_tester;
  // Stop syncing.
  {
    test::ThemeServiceChangedWaiter waiter(theme_service());
    theme_sync_service()->StopSyncing(syncer::THEMES);
    waiter.WaitForThemeChanged();
  }
  EXPECT_FALSE(theme_service()->UsingExtensionTheme());
  EXPECT_EQ(
      profile()->GetPrefs()->GetDict(prefs::kNtpCustomBackgroundDict),
      base::Value::Dict()
          .Set(kNtpCustomBackgroundURL, kTestUrl)
          .Set(kNtpCustomBackgroundAttributionLine1, "attribution_line_1")
          .Set(kNtpCustomBackgroundAttributionLine2, "attribution_line_2")
          .Set(kNtpCustomBackgroundAttributionActionURL,
               "attribution_action_url")
          .Set(kNtpCustomBackgroundCollectionId, "collection_id")
          .Set(kNtpCustomBackgroundResumeToken, "resume_token")
          .Set(kNtpCustomBackgroundRefreshTimestamp,
               static_cast<int>(1234567890))
          .Set(kNtpCustomBackgroundMainColor, static_cast<int>(SK_ColorRED)));
  histogram_tester.ExpectUniqueSample("Theme.RestoredLocalThemeUponSignout",
                                      true, 1);
}

TEST_F(ThemeSyncableServiceTestWithAccountThemesSeparation,
       LoadsDefaultThemeUponSyncStopIfNoLocalThemeExistedInPref) {
  // Set remote extension theme.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(true);
  theme_specifics.set_custom_theme_id(theme_extension()->id());
  theme_specifics.set_custom_theme_name(kCustomThemeName);
  theme_specifics.set_custom_theme_update_url(kCustomThemeUrl);

  // Start syncing.
  {
    test::ThemeServiceChangedWaiter waiter(theme_service());
    std::optional<syncer::ModelError> error =
        theme_sync_service()->MergeDataAndStartSyncing(
            syncer::THEMES, MakeThemeDataList(theme_specifics),
            std::unique_ptr<syncer::SyncChangeProcessor>(
                new syncer::SyncChangeProcessorWrapperForTest(
                    fake_change_processor())));
    ASSERT_FALSE(error.has_value()) << error.value().message();
    waiter.WaitForThemeChanged();
  }
  EXPECT_TRUE(theme_service()->UsingExtensionTheme());
  EXPECT_FALSE(
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict));

  base::HistogramTester histogram_tester;
  // Stop syncing.
  {
    test::ThemeServiceChangedWaiter waiter(theme_service());
    theme_sync_service()->StopSyncing(syncer::THEMES);
    waiter.WaitForThemeChanged();
  }
  EXPECT_FALSE(theme_service()->UsingExtensionTheme());
  EXPECT_TRUE(theme_service()->UsingDefaultTheme());
  histogram_tester.ExpectUniqueSample("Theme.RestoredLocalThemeUponSignout",
                                      false, 1);
}

TEST_F(ThemeSyncableServiceTestWithAccountThemesSeparation,
       ShouldNotLoadLocalThemeFromPrefUponBrowserShutdown) {
  // Set remote extension theme.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(false);
  theme_specifics.mutable_autogenerated_color_theme()->set_color(SK_ColorBLUE);

  // Start syncing.
  std::optional<syncer::ModelError> error =
      theme_sync_service()->MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::unique_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor())));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  EXPECT_TRUE(theme_service()->UsingAutogeneratedTheme());
  EXPECT_NE(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);

  // Set saved local theme pref.
  sync_pb::ThemeSpecifics local_theme_specifics;
  local_theme_specifics.set_use_custom_theme(false);
  sync_pb::ThemeSpecifics::UserColorTheme* user_color_theme =
      local_theme_specifics.mutable_user_color_theme();
  user_color_theme->set_color(SK_ColorRED);
  user_color_theme->set_browser_color_variant(
      sync_pb::ThemeSpecifics_UserColorTheme_BrowserColorVariant_TONAL_SPOT);

  profile()->GetPrefs()->SetString(
      prefs::kSavedLocalTheme,
      base::Base64Encode(local_theme_specifics.SerializeAsString()));

  base::HistogramTester histogram_tester;
  // Browser shutdown.
  theme_sync_service()->OnBrowserShutdown(syncer::THEMES);

  // Theme remains the same.
  EXPECT_TRUE(theme_service()->UsingAutogeneratedTheme());
  EXPECT_NE(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  histogram_tester.ExpectTotalCount("Theme.RestoredLocalThemeUponSignout", 0);
}

class ThemeSyncableServiceTestForThemeExtension
    : public ThemeSyncableServiceTestWithAccountThemesSeparation {
 public:
  void SetUp() override {
    ThemeSyncableServiceTestWithAccountThemesSeparation::SetUp();

    // Remove theme extension added during parent SetUp().
    service_->UnloadAllExtensionsForTest();
    ASSERT_FALSE(
        extensions::ExtensionRegistry::Get(profile())->GetExtensionById(
            kCustomThemeId, extensions::ExtensionRegistry::EVERYTHING));
    ASSERT_FALSE(extensions::PendingExtensionManager::Get(profile())
                     ->HasPendingExtensions());

    pending_extension_manager_ =
        extensions::PendingExtensionManager::Get(profile());
    extension_registry_ = extensions::ExtensionRegistry::Get(profile());
  }

  void InstallExtension() {
    registrar()->OnExtensionInstalled(
        theme_extension(), syncer::StringOrdinal(),
        extensions::kInstallFlagInstallImmediately);
    EXPECT_TRUE(base::test::RunUntil(
        [&]() { return theme_service()->UsingExtensionTheme(); }));
    EXPECT_TRUE(extensions::ExtensionRegistry::Get(profile())->GetExtensionById(
        kCustomThemeId, extensions::ExtensionRegistry::EVERYTHING));
  }

 protected:
  raw_ptr<extensions::PendingExtensionManager> pending_extension_manager_ =
      nullptr;
  raw_ptr<extensions::ExtensionRegistry> extension_registry_ = nullptr;
};

TEST_F(ThemeSyncableServiceTestForThemeExtension,
       ShouldRemovePendingThemeExtensionUponSignout) {
  // Set remote theme extension.
  sync_pb::ThemeSpecifics theme_specifics =
      theme_service::test::CreateThemeSpecificsWithExtensionTheme(
          kCustomThemeId, kCustomThemeName, kCustomThemeUrl);

  // Start syncing.
  theme_sync_service()->WillStartInitialSync();
  ASSERT_FALSE(theme_sync_service()->MergeDataAndStartSyncing(
      syncer::THEMES, MakeThemeDataList(theme_specifics),
      std::unique_ptr<syncer::SyncChangeProcessor>(
          new syncer::SyncChangeProcessorWrapperForTest(
              fake_change_processor()))));

  // The theme extension is pending install, hence the current theme is still
  // the default theme.
  EXPECT_TRUE(theme_service()->UsingDefaultTheme());
  EXPECT_TRUE(pending_extension_manager_->IsIdPending(kCustomThemeId));

  // Stop syncing.
  theme_sync_service()->StopSyncing(syncer::THEMES);
  EXPECT_TRUE(theme_service()->UsingDefaultTheme());
  // The pending theme extension should be cleared.
  EXPECT_FALSE(pending_extension_manager_->HasPendingExtensions());
}

TEST_F(ThemeSyncableServiceTestForThemeExtension,
       ShouldRemoveInstalledThemeExtensionUponSignout) {
  // Set remote theme extension.
  sync_pb::ThemeSpecifics theme_specifics =
      theme_service::test::CreateThemeSpecificsWithExtensionTheme(
          kCustomThemeId, kCustomThemeName, kCustomThemeUrl);

  // Start syncing.
  theme_sync_service()->WillStartInitialSync();
  ASSERT_FALSE(theme_sync_service()->MergeDataAndStartSyncing(
      syncer::THEMES, MakeThemeDataList(theme_specifics),
      std::unique_ptr<syncer::SyncChangeProcessor>(
          new syncer::SyncChangeProcessorWrapperForTest(
              fake_change_processor()))));

  // Simulate installing theme extension.
  InstallExtension();

  // Stop syncing.
  theme_sync_service()->StopSyncing(syncer::THEMES);
  EXPECT_TRUE(theme_service()->UsingDefaultTheme());
  // The extension was removed.
  EXPECT_FALSE(extension_registry_->GetExtensionById(
      kCustomThemeId, extensions::ExtensionRegistry::EVERYTHING));
}

// This tests that remote theme extension is neither installed nor removed upon
// signout if the same theme extension was already applied.
TEST_F(ThemeSyncableServiceTestForThemeExtension,
       ShouldNotRemoveThemeExtensionUponSignoutIfPreexistingTheme) {
  InstallExtension();

  // Set the same remote theme extension.
  sync_pb::ThemeSpecifics theme_specifics =
      theme_service::test::CreateThemeSpecificsWithExtensionTheme(
          kCustomThemeId, kCustomThemeName, kCustomThemeUrl);

  // Start syncing.
  theme_sync_service()->WillStartInitialSync();
  ASSERT_FALSE(theme_sync_service()->MergeDataAndStartSyncing(
      syncer::THEMES, MakeThemeDataList(theme_specifics),
      std::unique_ptr<syncer::SyncChangeProcessor>(
          new syncer::SyncChangeProcessorWrapperForTest(
              fake_change_processor()))));

  // Theme extension is already applied and doesn't need installation.
  ASSERT_TRUE(theme_service()->UsingExtensionTheme());
  EXPECT_FALSE(pending_extension_manager_->IsIdPending(kCustomThemeId));

  // Stop syncing.
  theme_sync_service()->StopSyncing(syncer::THEMES);
  EXPECT_TRUE(theme_service()->UsingExtensionTheme());
  // The extension was not removed.
  EXPECT_TRUE(extension_registry_->GetExtensionById(
      kCustomThemeId, extensions::ExtensionRegistry::EVERYTHING));
}

// This tests that remote theme extension is neither installed nor removed upon
// signout if the theme extension already exists (for example, if the extension
// is disabled).
TEST_F(ThemeSyncableServiceTestForThemeExtension,
       ShouldNotRemoveThemeExtensionUponSignoutIfPreexisting) {
  // Theme extension pre-exists but is disabled.
  InstallExtension();
  registrar()->DisableExtension(
      kCustomThemeId, {extensions::disable_reason::DISABLE_USER_ACTION});
  ASSERT_TRUE(extension_registry_->GetExtensionById(
      kCustomThemeId, extensions::ExtensionRegistry::EVERYTHING));
  ASSERT_FALSE(theme_service()->UsingExtensionTheme());

  // Set the same remote theme extension.
  sync_pb::ThemeSpecifics theme_specifics =
      theme_service::test::CreateThemeSpecificsWithExtensionTheme(
          kCustomThemeId, kCustomThemeName, kCustomThemeUrl);

  // Start syncing.
  theme_sync_service()->WillStartInitialSync();
  ASSERT_FALSE(theme_sync_service()->MergeDataAndStartSyncing(
      syncer::THEMES, MakeThemeDataList(theme_specifics),
      std::unique_ptr<syncer::SyncChangeProcessor>(
          new syncer::SyncChangeProcessorWrapperForTest(
              fake_change_processor()))));
  // Theme extension doesn't need installation.
  EXPECT_FALSE(pending_extension_manager_->IsIdPending(kCustomThemeId));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return theme_service()->UsingExtensionTheme(); }));

  // Stop syncing.
  theme_sync_service()->StopSyncing(syncer::THEMES);
  // The extension was not removed.
  EXPECT_TRUE(extension_registry_->GetExtensionById(
      kCustomThemeId, extensions::ExtensionRegistry::EVERYTHING));
}

class ThemePrefsMigrationTest : public ::testing::Test {
 public:
  void SetUp() override {
    user_prefs::PrefRegistrySyncable* registry = pref_service_.registry();
    // Register all the prefs involved in the migration.
    registry->RegisterBooleanPref(prefs::kSyncingThemePrefsMigratedToNonSyncing,
                                  false);
    registry->RegisterIntegerPref(
        prefs::kDeprecatedBrowserColorSchemeDoNotUse,
        static_cast<int>(ThemeService::BrowserColorScheme::kSystem),
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    registry->RegisterIntegerPref(
        prefs::kBrowserColorScheme,
        static_cast<int>(ThemeService::BrowserColorScheme::kSystem));
    registry->RegisterIntegerPref(
        prefs::kDeprecatedUserColorDoNotUse, SK_ColorTRANSPARENT,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    registry->RegisterIntegerPref(prefs::kUserColor, SK_ColorTRANSPARENT);
    registry->RegisterIntegerPref(
        prefs::kDeprecatedBrowserColorVariantDoNotUse,
        static_cast<int>(ui::mojom::BrowserColorVariant::kSystem),
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    registry->RegisterIntegerPref(
        prefs::kBrowserColorVariant,
        static_cast<int>(ui::mojom::BrowserColorVariant::kSystem));
    registry->RegisterBooleanPref(
        prefs::kDeprecatedGrayscaleThemeEnabledDoNotUse, false,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    registry->RegisterBooleanPref(prefs::kGrayscaleThemeEnabled, false);
    registry->RegisterDictionaryPref(
        prefs::kDeprecatedNtpCustomBackgroundDictDoNotUse);
    registry->RegisterDictionaryPref(prefs::kNtpCustomBackgroundDict);
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
};

TEST_F(ThemePrefsMigrationTest, MigrateSyncingThemePrefsToNonSyncing) {
  ASSERT_FALSE(
      pref_service_.GetBoolean(prefs::kSyncingThemePrefsMigratedToNonSyncing));

  pref_service_.SetInteger(prefs::kDeprecatedUserColorDoNotUse, SK_ColorBLUE);
  EXPECT_FALSE(pref_service_.HasPrefPath(prefs::kUserColor));

  base::HistogramTester histogram_tester;
  MigrateSyncingThemePrefsToNonSyncingIfNeeded(&pref_service_);
  EXPECT_TRUE(
      pref_service_.GetBoolean(prefs::kSyncingThemePrefsMigratedToNonSyncing));
  EXPECT_EQ(pref_service_.GetInteger(prefs::kUserColor),
            static_cast<int>(SK_ColorBLUE));
  histogram_tester.ExpectUniqueSample(
      kThemePrefMigrationAlreadyMigratedHistogram, false, 1);
  histogram_tester.ExpectUniqueSample(kThemePrefMigrationMigratedPrefHistogram,
                                      ThemePrefInMigration::kUserColor, 1);
}

TEST_F(ThemePrefsMigrationTest, MigrateSyncingNtpPrefToNonSyncing) {
  ASSERT_FALSE(
      pref_service_.GetBoolean(prefs::kSyncingThemePrefsMigratedToNonSyncing));

  pref_service_.SetInteger(prefs::kDeprecatedUserColorDoNotUse, SK_ColorBLUE);
  EXPECT_FALSE(pref_service_.HasPrefPath(prefs::kUserColor));

  pref_service_.SetDict(
      prefs::kDeprecatedNtpCustomBackgroundDictDoNotUse,
      base::Value::Dict()
          .Set(kNtpCustomBackgroundURL, kTestUrl)
          .Set(kNtpCustomBackgroundAttributionLine1, "attribution_line_1")
          .Set(kNtpCustomBackgroundAttributionLine2, "attribution_line_2")
          .Set(kNtpCustomBackgroundAttributionActionURL,
               "attribution_action_url")
          .Set(kNtpCustomBackgroundCollectionId, "collection_id")
          .Set(kNtpCustomBackgroundResumeToken, "resume_token")
          .Set(kNtpCustomBackgroundRefreshTimestamp,
               static_cast<int>(1234567890))
          .Set(kNtpCustomBackgroundMainColor, static_cast<int>(SK_ColorRED)));

  base::HistogramTester histogram_tester;
  MigrateSyncingThemePrefsToNonSyncingIfNeeded(&pref_service_);
  EXPECT_TRUE(
      pref_service_.GetBoolean(prefs::kSyncingThemePrefsMigratedToNonSyncing));
  EXPECT_EQ(pref_service_.GetInteger(prefs::kUserColor),
            static_cast<int>(SK_ColorBLUE));
  histogram_tester.ExpectUniqueSample(
      kThemePrefMigrationAlreadyMigratedHistogram, false, 1);
  histogram_tester.ExpectBucketCount(kThemePrefMigrationMigratedPrefHistogram,
                                     ThemePrefInMigration::kUserColor, 1);
  histogram_tester.ExpectBucketCount(
      kThemePrefMigrationMigratedPrefHistogram,
      ThemePrefInMigration::kNtpCustomBackgroundDict, 1);
}

TEST_F(ThemePrefsMigrationTest,
       DoNotMigrateSyncingThemePrefsToNonSyncingIfAlreadyDone) {
  pref_service_.SetBoolean(prefs::kSyncingThemePrefsMigratedToNonSyncing, true);
  pref_service_.SetInteger(prefs::kDeprecatedUserColorDoNotUse, SK_ColorBLUE);

  base::HistogramTester histogram_tester;
  MigrateSyncingThemePrefsToNonSyncingIfNeeded(&pref_service_);
  EXPECT_FALSE(pref_service_.HasPrefPath(prefs::kUserColor));
  histogram_tester.ExpectUniqueSample(
      kThemePrefMigrationAlreadyMigratedHistogram, true, 1);
  histogram_tester.ExpectTotalCount(kThemePrefMigrationMigratedPrefHistogram,
                                    0);
}

class ThemePrefsMigrationShouldReadPrefsTest : public ::testing::Test {
 public:
  ThemePrefsMigrationShouldReadPrefsTest() {
    profile_ = std::make_unique<TestingProfile>();
    // TestingProfile init automatically leads to creation of
    // ThemeSyncableService. To allow for more control for tests, reset the
    // ThemeSyncableService instance.
    theme_service()->ResetThemeSyncableServiceForTest();

    prefs()->SetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs, true);
  }

  syncer::SyncDataList InitialPrefsSyncData() {
    syncer::SyncDataList initial_data;
    initial_data.push_back(CreateRemotePrefsSyncData(
        prefs::kDeprecatedBrowserColorSchemeDoNotUse,
        base::Value(
            static_cast<int>(ThemeService::BrowserColorScheme::kLight))));
    initial_data.push_back(
        CreateRemotePrefsSyncData(prefs::kDeprecatedUserColorDoNotUse,
                                  base::Value(static_cast<int>(SK_ColorRED))));
    initial_data.push_back(CreateRemotePrefsSyncData(
        prefs::kDeprecatedBrowserColorVariantDoNotUse,
        base::Value(
            static_cast<int>(ui::mojom::BrowserColorVariant::kTonalSpot))));
    return initial_data;
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile_->GetTestingPrefService();
  }

  ThemeService* theme_service() {
    return ThemeServiceFactory::GetForProfile(profile_.get());
  }

 protected:
  syncer::SyncData CreateRemotePrefsSyncData(const std::string& name,
                                             const base::Value& value) {
    std::string serialized;
    JSONStringValueSerializer json(&serialized);
    EXPECT_TRUE(json.Serialize(value));
    sync_pb::EntitySpecifics entity_specifics;
    sync_pb::PreferenceSpecifics* specifics =
        entity_specifics.mutable_preference();
    specifics->set_name(name);
    specifics->set_value(serialized);
    return syncer::SyncData::CreateRemoteData(
        entity_specifics, syncer::ClientTagHash::FromUnhashed(
                              syncer::DataType::PREFERENCES, name));
  }

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<syncer::FakeSyncChangeProcessor> fake_change_processor_ =
      std::make_unique<syncer::FakeSyncChangeProcessor>();
  std::unique_ptr<TestingProfile> profile_;
};

// Verifies that syncing prefs are read upon construction of
// ThemeSyncableService if prefs sync has already started.
TEST_F(ThemePrefsMigrationShouldReadPrefsTest,
       ShouldReadThemePrefsOnContructionIfPrefsAlreadySyncing) {
  ASSERT_TRUE(prefs()->GetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs));

  // Start prefs sync.
  syncer::SyncableService* pref_sync_service =
      prefs()->GetSyncableService(syncer::PREFERENCES);
  ASSERT_FALSE(pref_sync_service->MergeDataAndStartSyncing(
      syncer::PREFERENCES, InitialPrefsSyncData(),
      std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
          fake_change_processor_.get())));

  ASSERT_TRUE(prefs()->IsSyncing());

  base::HistogramTester histogram_tester;
  ThemeSyncableService theme_syncable_service(profile_.get(), theme_service());

  // Syncing prefs were copied.
  EXPECT_FALSE(
      prefs()->GetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs));
  EXPECT_EQ(prefs()->GetInteger(prefs::kBrowserColorScheme),
            static_cast<int>(ThemeService::BrowserColorScheme::kLight));
  EXPECT_EQ(prefs()->GetInteger(prefs::kUserColor),
            static_cast<int>(SK_ColorRED));
  EXPECT_EQ(prefs()->GetInteger(prefs::kBrowserColorVariant),
            static_cast<int>(ui::mojom::BrowserColorVariant::kTonalSpot));

  // The applied prefs were logged.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          kThemePrefMigrationIncomingSyncingPrefAppliedHistogram),
      testing::ElementsAre(
          base::Bucket(
              static_cast<int>(ThemePrefInMigration::kBrowserColorScheme), 1),
          base::Bucket(static_cast<int>(ThemePrefInMigration::kUserColor), 1),
          base::Bucket(
              static_cast<int>(ThemePrefInMigration::kBrowserColorVariant),
              1)));
}

// Verifies that syncing theme prefs are read when prefs sync starts.
TEST_F(ThemePrefsMigrationShouldReadPrefsTest,
       ShouldReadThemePrefsWhenPrefsStartSyncing) {
  ASSERT_TRUE(prefs()->GetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs));

  ASSERT_FALSE(prefs()->IsSyncing());
  base::HistogramTester histogram_tester;
  ThemeSyncableService theme_syncable_service(profile_.get(), theme_service());

  // No pref logged yet since prefs hasn't started syncing.
  histogram_tester.ExpectTotalCount(
      kThemePrefMigrationIncomingSyncingPrefAppliedHistogram, 0);

  // Start prefs sync.
  syncer::SyncableService* pref_sync_service =
      prefs()->GetSyncableService(syncer::PREFERENCES);
  ASSERT_FALSE(pref_sync_service->MergeDataAndStartSyncing(
      syncer::PREFERENCES, InitialPrefsSyncData(),
      std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
          fake_change_processor_.get())));

  // Syncing prefs have been copied.
  EXPECT_FALSE(
      prefs()->GetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs));
  EXPECT_EQ(prefs()->GetInteger(prefs::kBrowserColorScheme),
            static_cast<int>(ThemeService::BrowserColorScheme::kLight));
  EXPECT_EQ(prefs()->GetInteger(prefs::kUserColor),
            static_cast<int>(SK_ColorRED));
  EXPECT_EQ(prefs()->GetInteger(prefs::kBrowserColorVariant),
            static_cast<int>(ui::mojom::BrowserColorVariant::kTonalSpot));

  // The applied prefs were logged.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          kThemePrefMigrationIncomingSyncingPrefAppliedHistogram),
      testing::ElementsAre(
          base::Bucket(
              static_cast<int>(ThemePrefInMigration::kBrowserColorScheme), 1),
          base::Bucket(static_cast<int>(ThemePrefInMigration::kUserColor), 1),
          base::Bucket(
              static_cast<int>(ThemePrefInMigration::kBrowserColorVariant),
              1)));
}

// Verifies that syncing theme prefs are not read if they have already been read
// before or if the migration flag has already been set.
TEST_F(ThemePrefsMigrationShouldReadPrefsTest,
       ShouldNotReadThemePrefsIfAlreadyRead) {
  // Mark as already read.
  prefs()->SetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs, false);

  base::HistogramTester histogram_tester;
  ThemeSyncableService theme_syncable_service(profile_.get(), theme_service());

  // Start prefs sync.
  syncer::SyncableService* pref_sync_service =
      prefs()->GetSyncableService(syncer::PREFERENCES);
  ASSERT_FALSE(pref_sync_service->MergeDataAndStartSyncing(
      syncer::PREFERENCES, InitialPrefsSyncData(),
      std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
          fake_change_processor_.get())));

  // Prefs are unchanged.
  EXPECT_FALSE(
      prefs()->GetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs));
  EXPECT_NE(prefs()->GetInteger(prefs::kBrowserColorScheme),
            static_cast<int>(ThemeService::BrowserColorScheme::kLight));
  EXPECT_NE(prefs()->GetInteger(prefs::kUserColor),
            static_cast<int>(SK_ColorRED));
  EXPECT_NE(prefs()->GetInteger(prefs::kBrowserColorVariant),
            static_cast<int>(ui::mojom::BrowserColorVariant::kTonalSpot));

  // No pref logged.
  histogram_tester.ExpectTotalCount(
      kThemePrefMigrationIncomingSyncingPrefAppliedHistogram, 0);
}

// Verifies that the migration flag is set and (thus) syncing prefs are not read
// if the incoming ThemeSpecifics contains the new fields.
TEST_F(ThemePrefsMigrationShouldReadPrefsTest,
       ShouldNotReadThemePrefsIfReadViaThemeSpecifics) {
  ASSERT_TRUE(prefs()->GetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs));

  ASSERT_FALSE(prefs()->IsSyncing());
  ThemeSyncableService theme_syncable_service(profile_.get(), theme_service());

  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_browser_color_scheme(
      sync_pb::ThemeSpecifics_BrowserColorScheme_DARK);

  // Start themes sync with specifics containing new field.
  std::optional<syncer::ModelError> error =
      theme_syncable_service.MergeDataAndStartSyncing(
          syncer::THEMES, MakeThemeDataList(theme_specifics),
          std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
              fake_change_processor_.get()));
  ASSERT_FALSE(error.has_value()) << error.value().message();

  // Migration flag is already set.
  EXPECT_FALSE(
      prefs()->GetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs));
  EXPECT_EQ(prefs()->GetInteger(prefs::kBrowserColorScheme),
            static_cast<int>(ThemeService::BrowserColorScheme::kDark));

  // Start prefs sync.
  syncer::SyncableService* pref_sync_service =
      prefs()->GetSyncableService(syncer::PREFERENCES);
  ASSERT_FALSE(pref_sync_service->MergeDataAndStartSyncing(
      syncer::PREFERENCES, InitialPrefsSyncData(),
      std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
          fake_change_processor_.get())));

  // Syncing prefs have not been copied since ThemeSpecifics had the new fields.
  EXPECT_FALSE(
      prefs()->GetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs));
  EXPECT_NE(prefs()->GetInteger(prefs::kBrowserColorScheme),
            static_cast<int>(ThemeService::BrowserColorScheme::kLight));
  EXPECT_NE(prefs()->GetInteger(prefs::kUserColor),
            static_cast<int>(SK_ColorRED));
  EXPECT_NE(prefs()->GetInteger(prefs::kBrowserColorVariant),
            static_cast<int>(ui::mojom::BrowserColorVariant::kTonalSpot));
}

// Verifies that syncing theme prefs are read if the incoming ThemeSpecifics
// didn't have the new fields.
TEST_F(ThemePrefsMigrationShouldReadPrefsTest,
       ShouldReadThemePrefsIfThemeSpecificsDoesNotHaveNewFields) {
  ASSERT_TRUE(prefs()->GetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs));

  ASSERT_FALSE(prefs()->IsSyncing());
  ThemeSyncableService theme_syncable_service(profile_.get(), theme_service());

  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.mutable_autogenerated_color_theme()->set_color(SK_ColorBLUE);

  // Start themes sync with specifics containing new field.
  ASSERT_FALSE(theme_syncable_service.MergeDataAndStartSyncing(
      syncer::THEMES, MakeThemeDataList(theme_specifics),
      std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
          fake_change_processor_.get())));

  EXPECT_TRUE(prefs()->GetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs));

  // Start prefs sync.
  syncer::SyncableService* pref_sync_service =
      prefs()->GetSyncableService(syncer::PREFERENCES);
  ASSERT_FALSE(pref_sync_service->MergeDataAndStartSyncing(
      syncer::PREFERENCES, InitialPrefsSyncData(),
      std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
          fake_change_processor_.get())));

  // Syncing prefs copied since ThemeSpecifics didn't have the new fields.
  EXPECT_FALSE(
      prefs()->GetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs));
  EXPECT_EQ(prefs()->GetInteger(prefs::kBrowserColorScheme),
            static_cast<int>(ThemeService::BrowserColorScheme::kLight));
  EXPECT_EQ(prefs()->GetInteger(prefs::kUserColor),
            static_cast<int>(SK_ColorRED));
  EXPECT_EQ(prefs()->GetInteger(prefs::kBrowserColorVariant),
            static_cast<int>(ui::mojom::BrowserColorVariant::kTonalSpot));
}

// Verifies that the incoming ThemeSpecifics overwrites the value copied from
// the syncing theme prefs.
TEST_F(ThemePrefsMigrationShouldReadPrefsTest, ShouldPrioritizeThemeSpecifics) {
  ASSERT_TRUE(prefs()->GetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs));

  ASSERT_FALSE(prefs()->IsSyncing());
  ThemeSyncableService theme_syncable_service(profile_.get(), theme_service());

  // Start prefs sync.
  syncer::SyncableService* pref_sync_service =
      prefs()->GetSyncableService(syncer::PREFERENCES);
  ASSERT_FALSE(pref_sync_service->MergeDataAndStartSyncing(
      syncer::PREFERENCES, InitialPrefsSyncData(),
      std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
          fake_change_processor_.get())));

  EXPECT_FALSE(
      prefs()->GetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs));
  EXPECT_EQ(prefs()->GetInteger(prefs::kBrowserColorScheme),
            static_cast<int>(ThemeService::BrowserColorScheme::kLight));

  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_browser_color_scheme(
      sync_pb::ThemeSpecifics_BrowserColorScheme_DARK);

  // Start themes sync with specifics containing new field.
  ASSERT_FALSE(theme_syncable_service.MergeDataAndStartSyncing(
      syncer::THEMES, MakeThemeDataList(theme_specifics),
      std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
          fake_change_processor_.get())));

  EXPECT_FALSE(
      prefs()->GetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs));
  // Overwrites the theme set by prefs.
  EXPECT_EQ(prefs()->GetInteger(prefs::kBrowserColorScheme),
            static_cast<int>(ThemeService::BrowserColorScheme::kDark));
}

// Regression test for crbug.com/375553464.
TEST_F(ThemePrefsMigrationShouldReadPrefsTest,
       ShouldOnlyNotifyOnceUponReadingThemePrefs) {
  ASSERT_FALSE(prefs()->IsSyncing());
  ThemeSyncableService theme_syncable_service(profile_.get(), theme_service());

  theme_service()->SetUserColorAndBrowserColorVariant(
      SK_ColorBLUE, ui::mojom::BrowserColorVariant::kNeutral);

  ASSERT_EQ(fake_change_processor_->changes().size(), 0u);

  // Start themes sync.
  ASSERT_FALSE(theme_syncable_service.MergeDataAndStartSyncing(
      syncer::THEMES, MakeThemeDataList(sync_pb::ThemeSpecifics()),
      std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
          fake_change_processor_.get())));

  ASSERT_EQ(fake_change_processor_->changes().size(),
            base::FeatureList::IsEnabled(syncer::kSeparateLocalAndAccountThemes)
                ? 0u
                : 1u);
  fake_change_processor_->changes().clear();

  ASSERT_EQ(prefs()->GetInteger(prefs::kUserColor),
            static_cast<int>(SK_ColorBLUE));
  ASSERT_EQ(prefs()->GetInteger(prefs::kBrowserColorVariant),
            static_cast<int>(ui::mojom::BrowserColorVariant::kNeutral));

  ASSERT_TRUE(prefs()->GetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs));
  // Start prefs sync.
  syncer::SyncableService* pref_sync_service =
      prefs()->GetSyncableService(syncer::PREFERENCES);
  ASSERT_FALSE(pref_sync_service->MergeDataAndStartSyncing(
      syncer::PREFERENCES, InitialPrefsSyncData(),
      std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
          fake_change_processor_.get())));

  EXPECT_EQ(1, std::ranges::count_if(
                   fake_change_processor_->changes(), [](const auto& e) {
                     return e.sync_data().GetSpecifics().has_theme();
                   }));
  EXPECT_EQ(prefs()->GetInteger(prefs::kUserColor),
            static_cast<int>(SK_ColorRED));
  EXPECT_EQ(prefs()->GetInteger(prefs::kBrowserColorVariant),
            static_cast<int>(ui::mojom::BrowserColorVariant::kTonalSpot));
}
