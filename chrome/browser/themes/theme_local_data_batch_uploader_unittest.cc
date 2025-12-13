// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_local_data_batch_uploader.h"

#include <string>

#include "base/files/file_path.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service_constants.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/themes/theme_service_test_utils.h"
#include "chrome/browser/themes/theme_service_utils.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "components/sync/protocol/theme_types.pb.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "components/sync/test/sync_change_processor_wrapper_for_test.h"
#include "components/sync/test/test_matchers.h"
#include "components/sync/test/test_sync_service.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/permissions/permission_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using base::test::EqualsProto;
using ::syncer::IsEmptyLocalDataDescription;
using ::syncer::MatchesLocalDataDescription;
using ::syncer::MatchesLocalDataItemModel;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Property;
using ::testing::SizeIs;
using theme_service::test::MakeThemeChangeList;
using theme_service::test::MakeThemeDataList;
using theme_service::test::MakeThemeExtension;

static const char* kCustomThemeId = "abcdefghijklmnopabcdefghijklmnop";
static const char kCustomThemeName[] = "name";
static const char kCustomThemeUrl[] = "http://update.url/foo";
constexpr char kTestUrl[] = "https://www.foo.com";

#if BUILDFLAG(IS_WIN)
const base::FilePath::CharType kExtensionFilePath[] =
    FILE_PATH_LITERAL("c:\\foo");
#elif BUILDFLAG(IS_POSIX)
const base::FilePath::CharType kExtensionFilePath[] = FILE_PATH_LITERAL("/oo");
#else
#error "Unknown platform"
#endif

// Returns a matcher that matches a SyncChangeList that contains a single
// ACTION_UPDATE change with the given ThemeSpecifics.
auto HasThemeSpecifics(const sync_pb::ThemeSpecifics& specifics) {
  return AllOf(
      Not(IsEmpty()),
      Property(
          &syncer::SyncChangeList::back,
          AllOf(Property(&syncer::SyncChange::change_type,
                         syncer::SyncChange::ACTION_UPDATE),
                Property(&syncer::SyncChange::sync_data,
                         Property(&syncer::SyncData::GetSpecifics,
                                  Property(&sync_pb::EntitySpecifics::theme,
                                           EqualsProto(specifics)))))));
}

class ThemeLocalDataBatchUploaderTestBase
    : public extensions::ExtensionServiceTestBase {
 protected:
  void SetUp() override {
    // Setting a matching update URL is necessary to make the test theme
    // considered syncable.
    extension_test_util::SetGalleryUpdateURL(GURL(kCustomThemeUrl));

    // Prevent theme preprocessing in tests to avoid errors.
    ThemeService::DisableThemePackForTesting();

    extensions::ExtensionServiceTestBase::SetUp();
    InitializeExtensionService(ExtensionServiceInitParams());
    service()->Init();

    theme_service_ = ThemeServiceFactory::GetForProfile(profile());
    theme_sync_service_ = theme_service_->GetThemeSyncableService();
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

    batch_uploader_ = std::make_unique<ThemeLocalDataBatchUploader>(
        theme_sync_service_.get());

    // Avoid using the real SyncService instance, to avoid triggering sync
    // startup notifications, specifically clearing of existing account data
    // upon startup when there is no sync metadata.
    // TODO(crbug.com/425913203): Remove once usage of TestSyncService is
    // simplified.
    SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<syncer::TestSyncService>();
        }));
  }

  ThemeService* theme_service() { return theme_service_; }

  ThemeSyncableService* theme_sync_service() { return theme_sync_service_; }

  syncer::FakeSyncChangeProcessor* fake_change_processor() {
    return fake_change_processor_.get();
  }

  const extensions::Extension* theme_extension() const {
    return theme_extension_.get();
  }

  syncer::LocalDataDescription GetLocalDataDescription() {
    syncer::LocalDataDescription description;
    base::test::TestFuture<syncer::LocalDataDescription> test_future_;
    batch_uploader_->GetLocalDataDescription(test_future_.GetCallback());
    return test_future_.Take();
  }

  void TriggerLocalDataMigration() {
    batch_uploader_->TriggerLocalDataMigration();
  }

  void TriggerLocalDataMigrationForItems(
      std::vector<syncer::LocalDataItemModel::DataId> items) {
    batch_uploader_->TriggerLocalDataMigrationForItems(items);
  }

  void StartSyncing(const sync_pb::ThemeSpecifics& theme_specifics) {
    theme_sync_service()->WillStartInitialSync();

    ASSERT_FALSE(theme_sync_service()->MergeDataAndStartSyncing(
        syncer::THEMES, MakeThemeDataList(theme_specifics),
        std::unique_ptr<syncer::SyncChangeProcessor>(
            new syncer::SyncChangeProcessorWrapperForTest(
                fake_change_processor()))));

    // Extension theme is applied asynchronously. Wait to detect a theme change
    // before exiting.
    if (theme_specifics.use_custom_theme()) {
      ASSERT_TRUE(base::test::RunUntil(
          [&]() { return theme_service()->UsingExtensionTheme(); }));
    }
  }

 private:
  raw_ptr<ThemeService> theme_service_;
  raw_ptr<ThemeSyncableService> theme_sync_service_;
  std::unique_ptr<syncer::FakeSyncChangeProcessor> fake_change_processor_;
  scoped_refptr<extensions::Extension> theme_extension_;
  std::unique_ptr<ThemeLocalDataBatchUploader> batch_uploader_;
};

class ThemeLocalDataBatchUploaderTestWithFlagDisabled
    : public ThemeLocalDataBatchUploaderTestBase {
 public:
  ThemeLocalDataBatchUploaderTestWithFlagDisabled() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{syncer::kSeparateLocalAndAccountThemes},
        /*disabled_features=*/{syncer::kThemesBatchUpload});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ThemeLocalDataBatchUploaderTestWithFlagDisabled, ShouldReturnNoItems) {
  // Local extension theme.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return theme_service()->UsingExtensionTheme(); }));

  const sync_pb::ThemeSpecifics local_theme_specifics =
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting();
  const sync_pb::ThemeSpecifics remote_theme_specifics =
      theme_service::test::CreateThemeSpecificsWithColorTheme();

  StartSyncing(remote_theme_specifics);

  EXPECT_THAT(GetLocalDataDescription(), IsEmptyLocalDataDescription());
}

using ThemeLocalDataBatchUploaderDeathTestWithFlagDisabled =
    ThemeLocalDataBatchUploaderTestWithFlagDisabled;

TEST_F(ThemeLocalDataBatchUploaderDeathTestWithFlagDisabled,
       TriggerLocalDataMigration) {
  // Local extension theme.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return theme_service()->UsingExtensionTheme(); }));

  const sync_pb::ThemeSpecifics local_theme_specifics =
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting();
  const sync_pb::ThemeSpecifics remote_theme_specifics =
      theme_service::test::CreateThemeSpecificsWithColorTheme();

  StartSyncing(remote_theme_specifics);

  EXPECT_DEATH(TriggerLocalDataMigration(), "");
}

class ThemeLocalDataBatchUploaderTest
    : public ThemeLocalDataBatchUploaderTestBase,
      public testing::WithParamInterface<sync_pb::ThemeSpecifics> {
 public:
  ThemeLocalDataBatchUploaderTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{syncer::kSeparateLocalAndAccountThemes,
                              syncer::kThemesBatchUpload},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ThemeLocalDataBatchUploaderTest,
       ShouldReturnEmptyUponGetLocalDataDescriptionForNoTheme) {
  ASSERT_THAT(
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
      EqualsProto(theme_service::test::EmptySpecifics()));

  EXPECT_THAT(GetLocalDataDescription(), IsEmptyLocalDataDescription());

  theme_sync_service()->WillStartInitialSync();
  ASSERT_FALSE(theme_sync_service()->MergeDataAndStartSyncing(
      syncer::THEMES, syncer::SyncDataList{},
      std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
          fake_change_processor())));

  base::HistogramTester histogram_tester;
  EXPECT_THAT(GetLocalDataDescription(), IsEmptyLocalDataDescription());
  histogram_tester.ExpectUniqueSample("Theme.BatchUpload.HasLocalTheme", false,
                                      1);
}

TEST_F(ThemeLocalDataBatchUploaderTest,
       ShouldReturnEmptyUponGetLocalDataDescriptionForAccountTheme) {
  // Set extension account theme.
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(true);
  theme_specifics.set_custom_theme_id(theme_extension()->id());
  theme_specifics.set_custom_theme_name(kCustomThemeName);
  theme_specifics.set_custom_theme_update_url(kCustomThemeUrl);

  theme_sync_service()->WillStartInitialSync();
  ASSERT_FALSE(theme_sync_service()->MergeDataAndStartSyncing(
      syncer::THEMES, MakeThemeDataList(theme_specifics),
      std::unique_ptr<syncer::SyncChangeProcessor>(
          new syncer::SyncChangeProcessorWrapperForTest(
              fake_change_processor()))));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return theme_service()->UsingExtensionTheme(); }));
  EXPECT_THAT(GetLocalDataDescription(), IsEmptyLocalDataDescription());

  // Set a non-extension account theme.
  theme_specifics.Clear();
  theme_specifics.set_use_custom_theme(false);
  theme_specifics.set_browser_color_scheme(
      ::sync_pb::ThemeSpecifics_BrowserColorScheme_SYSTEM);
  sync_pb::UserColorTheme* user_color_theme =
      theme_specifics.mutable_user_color_theme();
  user_color_theme->set_color(SK_ColorRED);
  user_color_theme->set_browser_color_variant(
      sync_pb::UserColorTheme_BrowserColorVariant_TONAL_SPOT);

  ASSERT_FALSE(theme_sync_service()->ProcessSyncChanges(
      FROM_HERE, MakeThemeChangeList(theme_specifics)));

  ASSERT_FALSE(theme_service()->UsingExtensionTheme());
  EXPECT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);

  base::HistogramTester histogram_tester;
  EXPECT_THAT(GetLocalDataDescription(), IsEmptyLocalDataDescription());
  histogram_tester.ExpectUniqueSample("Theme.BatchUpload.HasLocalTheme", false,
                                      1);
}

TEST_F(ThemeLocalDataBatchUploaderTest, TriggerLocalDataMigrationForNoItem) {
  // Local grayscale theme.
  theme_service()->SetIsGrayscale(true);
  ASSERT_TRUE(theme_service()->GetIsGrayscale());

  const sync_pb::ThemeSpecifics local_theme_specifics =
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting();
  const sync_pb::ThemeSpecifics remote_theme_specifics =
      theme_service::test::CreateThemeSpecificsWithAutogeneratedColorTheme();

  StartSyncing(remote_theme_specifics);

  base::HistogramTester histogram_tester;
  syncer::LocalDataDescription desc = GetLocalDataDescription();
  EXPECT_EQ(desc.type, syncer::THEMES);
  EXPECT_THAT(desc.local_data_models, SizeIs(1));
  histogram_tester.ExpectUniqueSample("Theme.BatchUpload.HasLocalTheme", true,
                                      1);

  ASSERT_FALSE(theme_service()->GetIsGrayscale());
  EXPECT_THAT(
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
      EqualsProto(remote_theme_specifics));

  TriggerLocalDataMigrationForItems(/*items=*/{});
  EXPECT_FALSE(theme_service()->GetIsGrayscale());
  // Nothing is committed.
  EXPECT_THAT(fake_change_processor()->changes(), IsEmpty());
  histogram_tester.ExpectTotalCount(
      "Theme.BatchUpload.LocalThemeMigrationTriggered", 0);

  theme_sync_service()->StopSyncing(syncer::THEMES);
  EXPECT_TRUE(theme_service()->GetIsGrayscale());
  histogram_tester.ExpectUniqueSample("Theme.RestoredLocalThemeUponSignout",
                                      true, 1);
}

TEST_F(ThemeLocalDataBatchUploaderTest,
       TriggerLocalDataMigrationForItemsForCorrectItem) {
  // Local grayscale theme.
  theme_service()->SetIsGrayscale(true);
  ASSERT_TRUE(theme_service()->GetIsGrayscale());

  const sync_pb::ThemeSpecifics local_theme_specifics =
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting();
  const sync_pb::ThemeSpecifics remote_theme_specifics =
      theme_service::test::CreateThemeSpecificsWithAutogeneratedColorTheme();

  StartSyncing(remote_theme_specifics);

  base::HistogramTester histogram_tester;
  TriggerLocalDataMigrationForItems(
      /*items=*/{ThemeLocalDataBatchUploader::kThemesLocalDataItemModelId});
  EXPECT_TRUE(theme_service()->GetIsGrayscale());
  EXPECT_THAT(
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
      EqualsProto(local_theme_specifics));
  // The local theme is committed.
  EXPECT_THAT(fake_change_processor()->changes(),
              HasThemeSpecifics(local_theme_specifics));
  histogram_tester.ExpectUniqueSample(
      "Theme.BatchUpload.LocalThemeMigrationTriggered", true, 1);

  // GetLocalDataDescription should now return empty.
  EXPECT_THAT(GetLocalDataDescription(), IsEmptyLocalDataDescription());
  histogram_tester.ExpectUniqueSample("Theme.BatchUpload.HasLocalTheme", false,
                                      1);

  theme_sync_service()->StopSyncing(syncer::THEMES);
  EXPECT_FALSE(theme_service()->GetIsGrayscale());
  histogram_tester.ExpectUniqueSample("Theme.RestoredLocalThemeUponSignout",
                                      false, 1);
}

TEST_P(ThemeLocalDataBatchUploaderTest, LocalExtensionTheme) {
  // Local extension theme.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return theme_service()->UsingExtensionTheme(); }));

  const sync_pb::ThemeSpecifics local_theme_specifics =
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting();
  const sync_pb::ThemeSpecifics remote_theme_specifics = GetParam();

  StartSyncing(remote_theme_specifics);

  EXPECT_THAT(GetLocalDataDescription(),
              MatchesLocalDataDescription(
                  syncer::DataType::THEMES,
                  ElementsAre(MatchesLocalDataItemModel(
                      ThemeLocalDataBatchUploader::kThemesLocalDataItemModelId,
                      syncer::LocalDataItemModel::NoIcon(),
                      /*title=*/kCustomThemeName, /*subtitle=*/IsEmpty())),
                  /*item_count=*/0u, /*domains=*/IsEmpty(),
                  /*domain_count=*/0u));

  // Skip the rest of the test if remote theme is the same.
  if (local_theme_specifics.SerializeAsString() ==
      remote_theme_specifics.SerializeAsString()) {
    return;
  }

  ASSERT_FALSE(theme_service()->UsingExtensionTheme());
  EXPECT_THAT(
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
      EqualsProto(remote_theme_specifics));

  TriggerLocalDataMigration();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return theme_service()->UsingExtensionTheme(); }));
  EXPECT_THAT(
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
      EqualsProto(local_theme_specifics));
  // The local theme is committed.
  EXPECT_THAT(fake_change_processor()->changes(),
              HasThemeSpecifics(local_theme_specifics));

  // GetLocalDataDescription should now return empty.
  EXPECT_THAT(GetLocalDataDescription(), IsEmptyLocalDataDescription());

  theme_sync_service()->StopSyncing(syncer::THEMES);
  EXPECT_TRUE(theme_service()->UsingDefaultTheme());
}

TEST_P(ThemeLocalDataBatchUploaderTest, LocalAutogeneratedColorTheme) {
  // Local autogenerated color theme.
  theme_service()->BuildAutogeneratedThemeFromColor(SK_ColorBLUE);
  ASSERT_TRUE(theme_service()->UsingAutogeneratedTheme());

  const sync_pb::ThemeSpecifics local_theme_specifics =
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting();
  const sync_pb::ThemeSpecifics remote_theme_specifics = GetParam();

  StartSyncing(remote_theme_specifics);

  EXPECT_THAT(GetLocalDataDescription(),
              MatchesLocalDataDescription(
                  syncer::DataType::THEMES,
                  ElementsAre(MatchesLocalDataItemModel(
                      ThemeLocalDataBatchUploader::kThemesLocalDataItemModelId,
                      syncer::LocalDataItemModel::NoIcon(),
                      /*title=*/"Custom color", /*subtitle=*/IsEmpty())),
                  /*item_count=*/0u, /*domains=*/IsEmpty(),
                  /*domain_count=*/0u));

  ASSERT_NE(theme_service()->GetAutogeneratedThemeColor(), SK_ColorBLUE);
  EXPECT_THAT(
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
      EqualsProto(remote_theme_specifics));

  TriggerLocalDataMigration();
  EXPECT_TRUE(theme_service()->UsingAutogeneratedTheme());
  EXPECT_EQ(theme_service()->GetAutogeneratedThemeColor(), SK_ColorBLUE);
  EXPECT_THAT(
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
      EqualsProto(local_theme_specifics));
  // The local theme is committed.
  EXPECT_THAT(fake_change_processor()->changes(),
              HasThemeSpecifics(local_theme_specifics));

  // GetLocalDataDescription should now return empty.
  EXPECT_THAT(GetLocalDataDescription(), IsEmptyLocalDataDescription());

  theme_sync_service()->StopSyncing(syncer::THEMES);
  EXPECT_TRUE(theme_service()->UsingDefaultTheme());
}

// If only a user color theme is set, no batch upload is offered. This is to
// catch cases where the color might have been set during profile creation and
// might accidentally lead to overwriting the current theme. This is a good
// enough trade-off to avoid the issue given that the user can easily set a user
// color theme manually again if they want to. See crbug.com/433935323 for more
// details.
TEST_P(ThemeLocalDataBatchUploaderTest, LocalUserColorTheme) {
  // Local user color theme.
  theme_service()->SetUserColorAndBrowserColorVariant(
      SK_ColorGREEN, ui::mojom::BrowserColorVariant::kTonalSpot);
  ASSERT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);

  const sync_pb::ThemeSpecifics local_theme_specifics =
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting();
  const sync_pb::ThemeSpecifics remote_theme_specifics = GetParam();

  StartSyncing(remote_theme_specifics);

  EXPECT_THAT(GetLocalDataDescription(), IsEmptyLocalDataDescription());

  ASSERT_NE(theme_service()->GetUserColor(), SK_ColorGREEN);
  EXPECT_THAT(
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
      EqualsProto(remote_theme_specifics));

  TriggerLocalDataMigration();
  EXPECT_NE(theme_service()->GetUserColor(), SK_ColorGREEN);
  EXPECT_THAT(
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
      EqualsProto(remote_theme_specifics));
  // The local theme is not committed.
  EXPECT_THAT(fake_change_processor()->changes(),
              Not(HasThemeSpecifics(local_theme_specifics)));

  theme_sync_service()->StopSyncing(syncer::THEMES);
  // The pre-existing user color theme is restored.
  EXPECT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  EXPECT_EQ(theme_service()->GetUserColor(), SK_ColorGREEN);
}

// If both a user color theme and a browser color scheme are set, the batch
// upload dialog should still be offered, unlike the cases where only one of
// these is set.
TEST_P(ThemeLocalDataBatchUploaderTest,
       LocalUserColorThemeAndBrowserColorScheme) {
  // Local user color theme and browser color scheme.
  theme_service()->SetUserColorAndBrowserColorVariant(
      SK_ColorGREEN, ui::mojom::BrowserColorVariant::kTonalSpot);
  theme_service()->SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kLight);
  ASSERT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);

  const sync_pb::ThemeSpecifics local_theme_specifics =
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting();
  const sync_pb::ThemeSpecifics remote_theme_specifics = GetParam();

  StartSyncing(remote_theme_specifics);

  EXPECT_THAT(GetLocalDataDescription(),
              MatchesLocalDataDescription(
                  syncer::DataType::THEMES,
                  ElementsAre(MatchesLocalDataItemModel(
                      ThemeLocalDataBatchUploader::kThemesLocalDataItemModelId,
                      syncer::LocalDataItemModel::NoIcon(),
                      /*title=*/"Green color", /*subtitle=*/IsEmpty())),
                  /*item_count=*/0u, /*domains=*/IsEmpty(),
                  /*domain_count=*/0u));

  ASSERT_NE(theme_service()->GetUserColor(), SK_ColorGREEN);
  EXPECT_THAT(
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
      EqualsProto(remote_theme_specifics));

  TriggerLocalDataMigration();
  EXPECT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  EXPECT_EQ(theme_service()->GetUserColor(), SK_ColorGREEN);
  EXPECT_EQ(theme_service()->GetBrowserColorScheme(),
            ThemeService::BrowserColorScheme::kLight);
  EXPECT_THAT(
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
      EqualsProto(local_theme_specifics));
  // The local theme is committed.
  EXPECT_THAT(fake_change_processor()->changes(),
              HasThemeSpecifics(local_theme_specifics));

  // GetLocalDataDescription should now return empty.
  EXPECT_THAT(GetLocalDataDescription(), IsEmptyLocalDataDescription());

  theme_sync_service()->StopSyncing(syncer::THEMES);
  EXPECT_TRUE(theme_service()->UsingDefaultTheme());
  EXPECT_NE(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);
  EXPECT_NE(theme_service()->GetUserColor(), SK_ColorGREEN);
  EXPECT_NE(theme_service()->GetBrowserColorScheme(),
            ThemeService::BrowserColorScheme::kLight);
}

TEST_P(ThemeLocalDataBatchUploaderTest, LocalGrayscaleTheme) {
  // Local grayscale theme.
  theme_service()->SetIsGrayscale(true);
  ASSERT_TRUE(theme_service()->GetIsGrayscale());

  const sync_pb::ThemeSpecifics local_theme_specifics =
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting();
  const sync_pb::ThemeSpecifics remote_theme_specifics = GetParam();

  StartSyncing(remote_theme_specifics);

  EXPECT_THAT(GetLocalDataDescription(),
              MatchesLocalDataDescription(
                  syncer::DataType::THEMES,
                  ElementsAre(MatchesLocalDataItemModel(
                      ThemeLocalDataBatchUploader::kThemesLocalDataItemModelId,
                      syncer::LocalDataItemModel::NoIcon(),
                      /*title=*/"Grey default color", /*subtitle=*/IsEmpty())),
                  /*item_count=*/0u, /*domains=*/IsEmpty(),
                  /*domain_count=*/0u));

  // Skip the rest of the test if remote theme is the same.
  if (local_theme_specifics.SerializeAsString() ==
      GetParam().SerializeAsString()) {
    return;
  }

  ASSERT_FALSE(theme_service()->GetIsGrayscale());
  EXPECT_THAT(
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
      EqualsProto(remote_theme_specifics));

  TriggerLocalDataMigration();
  EXPECT_TRUE(theme_service()->GetIsGrayscale());
  EXPECT_THAT(
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
      EqualsProto(local_theme_specifics));
  // The local theme is committed.
  EXPECT_THAT(fake_change_processor()->changes(),
              HasThemeSpecifics(local_theme_specifics));

  // GetLocalDataDescription should now return empty.
  EXPECT_THAT(GetLocalDataDescription(), IsEmptyLocalDataDescription());

  theme_sync_service()->StopSyncing(syncer::THEMES);
  EXPECT_TRUE(theme_service()->UsingDefaultTheme());
}

TEST_P(ThemeLocalDataBatchUploaderTest, LocalNtpBackground) {
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

  const sync_pb::ThemeSpecifics local_theme_specifics =
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting();
  const sync_pb::ThemeSpecifics remote_theme_specifics = GetParam();

  StartSyncing(remote_theme_specifics);

  EXPECT_THAT(GetLocalDataDescription(),
              MatchesLocalDataDescription(
                  syncer::DataType::THEMES,
                  ElementsAre(MatchesLocalDataItemModel(
                      ThemeLocalDataBatchUploader::kThemesLocalDataItemModelId,
                      syncer::LocalDataItemModel::NoIcon(),
                      /*title=*/"attribution_line_1", /*subtitle=*/IsEmpty())),
                  /*item_count=*/0u, /*domains=*/IsEmpty(),
                  /*domain_count=*/0u));

  EXPECT_NE(profile()->GetPrefs()->GetDict(prefs::kNtpCustomBackgroundDict),
            background_dict);
  EXPECT_THAT(
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
      EqualsProto(remote_theme_specifics));

  TriggerLocalDataMigration();
  EXPECT_EQ(profile()->GetPrefs()->GetDict(prefs::kNtpCustomBackgroundDict),
            background_dict);
  EXPECT_THAT(
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
      EqualsProto(local_theme_specifics));
  // The local theme is committed.
  EXPECT_THAT(fake_change_processor()->changes(),
              HasThemeSpecifics(local_theme_specifics));

  // GetLocalDataDescription should now return empty.
  EXPECT_THAT(GetLocalDataDescription(), IsEmptyLocalDataDescription());

  theme_sync_service()->StopSyncing(syncer::THEMES);
  EXPECT_FALSE(
      profile()->GetPrefs()->GetUserPrefValue(prefs::kNtpCustomBackgroundDict));
  EXPECT_TRUE(theme_service()->UsingDefaultTheme());
}

TEST_P(ThemeLocalDataBatchUploaderTest, LocalBrowserColorScheme) {
  // Local browser color scheme.
  theme_service()->SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kLight);

  const sync_pb::ThemeSpecifics local_theme_specifics =
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting();
  sync_pb::ThemeSpecifics remote_theme_specifics = GetParam();

  StartSyncing(remote_theme_specifics);

  // Just a browser color scheme by itself is considered equivalent to a default
  // theme and should not be offered for batch upload.
  EXPECT_THAT(GetLocalDataDescription(), IsEmptyLocalDataDescription());

  EXPECT_THAT(
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
      EqualsProto(remote_theme_specifics));

  TriggerLocalDataMigration();
  EXPECT_THAT(
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
      EqualsProto(remote_theme_specifics));
  // The local theme is not committed.
  EXPECT_THAT(fake_change_processor()->changes(),
              Not(HasThemeSpecifics(local_theme_specifics)));

  theme_sync_service()->StopSyncing(syncer::THEMES);
  EXPECT_EQ(theme_service()->GetBrowserColorScheme(),
            ThemeService::BrowserColorScheme::kLight);
}

TEST_P(ThemeLocalDataBatchUploaderTest, LocalSystemTheme) {
  // Local system theme.
  if (!theme_service()->IsSystemThemeDistinctFromDefaultTheme()) {
    return;
  }
  theme_service()->UseSystemTheme();
  ASSERT_TRUE(theme_service()->UsingSystemTheme());

  const sync_pb::ThemeSpecifics local_theme_specifics =
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting();
  const sync_pb::ThemeSpecifics remote_theme_specifics = GetParam();

  StartSyncing(remote_theme_specifics);

  EXPECT_THAT(GetLocalDataDescription(), IsEmptyLocalDataDescription());

  ASSERT_FALSE(theme_service()->UsingSystemTheme());
  EXPECT_THAT(
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
      EqualsProto(remote_theme_specifics));

  TriggerLocalDataMigration();
  EXPECT_THAT(
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
      EqualsProto(remote_theme_specifics));
  // The local theme is not committed.
  EXPECT_THAT(fake_change_processor()->changes(),
              Not(HasThemeSpecifics(local_theme_specifics)));

  theme_sync_service()->StopSyncing(syncer::THEMES);
  EXPECT_TRUE(theme_service()->UsingSystemTheme());
}

TEST_P(ThemeLocalDataBatchUploaderTest, LocalDefaultTheme) {
  // Set local default theme.
  theme_service()->UseDefaultTheme();
  ASSERT_TRUE(theme_service()->UsingDefaultTheme());

  const sync_pb::ThemeSpecifics local_theme_specifics =
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting();
  const sync_pb::ThemeSpecifics remote_theme_specifics = GetParam();

  StartSyncing(remote_theme_specifics);

  EXPECT_THAT(GetLocalDataDescription(), IsEmptyLocalDataDescription());

  EXPECT_THAT(
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
      EqualsProto(remote_theme_specifics));

  TriggerLocalDataMigration();
  EXPECT_THAT(
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting(),
      EqualsProto(remote_theme_specifics));
  // The local theme is not committed.
  EXPECT_THAT(fake_change_processor()->changes(),
              Not(HasThemeSpecifics(local_theme_specifics)));

  theme_sync_service()->StopSyncing(syncer::THEMES);
  EXPECT_TRUE(theme_service()->UsingDefaultTheme());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ThemeLocalDataBatchUploaderTest,
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

using ThemeLocalDataBatchUploaderDeathTest = ThemeLocalDataBatchUploaderTest;

TEST_F(ThemeLocalDataBatchUploaderDeathTest,
       TriggerLocalDataMigrationForItemsWithMoreThanOneItem) {
  const std::string kExpectedErrorMessageHint =
      "Only one local theme can exist";

  // Local user color theme.
  theme_service()->SetUserColorAndBrowserColorVariant(
      SK_ColorBLUE, ui::mojom::BrowserColorVariant::kTonalSpot);
  ASSERT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);

  const sync_pb::ThemeSpecifics local_theme_specifics =
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting();
  const sync_pb::ThemeSpecifics remote_theme_specifics =
      theme_service::test::CreateThemeSpecificsWithAutogeneratedColorTheme();

  StartSyncing(remote_theme_specifics);

  EXPECT_DEATH(
      TriggerLocalDataMigrationForItems(
          /*items=*/{ThemeLocalDataBatchUploader::kThemesLocalDataItemModelId,
                     "another-theme"}),
      "");
}

TEST_F(ThemeLocalDataBatchUploaderDeathTest,
       TriggerLocalDataMigrationForItemsWithInvalidItemIdType) {
  const std::string kExpectedErrorMessageHint = "Invalid item ID type";
  const syncer::LocalDataItemModel::DataId illegal_item = 1;

  // Local user color theme.
  theme_service()->SetUserColorAndBrowserColorVariant(
      SK_ColorBLUE, ui::mojom::BrowserColorVariant::kTonalSpot);
  ASSERT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);

  const sync_pb::ThemeSpecifics local_theme_specifics =
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting();
  const sync_pb::ThemeSpecifics remote_theme_specifics =
      theme_service::test::CreateThemeSpecificsWithAutogeneratedColorTheme();

  StartSyncing(remote_theme_specifics);

  EXPECT_DEATH(TriggerLocalDataMigrationForItems(/*items=*/{illegal_item}), "");
}

TEST_F(ThemeLocalDataBatchUploaderDeathTest,
       TriggerLocalDataMigrationForItemsWithInvalidItemId) {
  const std::string kExpectedErrorMessageHint = "Invalid item ID";
  const syncer::LocalDataItemModel::DataId illegal_item = "another-theme";

  // Local user color theme.
  theme_service()->SetUserColorAndBrowserColorVariant(
      SK_ColorBLUE, ui::mojom::BrowserColorVariant::kTonalSpot);
  ASSERT_EQ(theme_service()->GetThemeID(), ThemeService::kUserColorThemeID);

  const sync_pb::ThemeSpecifics local_theme_specifics =
      theme_sync_service()->GetThemeSpecificsFromCurrentThemeForTesting();
  const sync_pb::ThemeSpecifics remote_theme_specifics =
      theme_service::test::CreateThemeSpecificsWithAutogeneratedColorTheme();

  StartSyncing(remote_theme_specifics);

  EXPECT_DEATH(TriggerLocalDataMigrationForItems(/*items=*/{illegal_item}), "");
}

}  // namespace
