// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/language_packs/language_pack_font_service.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/language_packs/language_pack_font_service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::language_packs {
namespace {

using testing::ElementsAre;
using testing::FieldsAre;
using testing::HasSubstr;
using testing::IsEmpty;
using testing::Property;
using testing::Return;
using testing::StartsWith;

// `FakeDlcserviceClient::Install` adds DLCs to the return value of
// `GetExistingDlcs`, so we use that to observe whether any DLCs have been
// installed.
using GetExistingDlcsTestFuture =
    base::test::TestFuture<const std::string&,
                           const dlcservice::DlcsWithContent&>;
using MockAddFontDir =
    testing::MockFunction<LanguagePackFontService::AddFontDir>;

class LanguagePackFontServiceTest : public testing::Test {
 public:
  LanguagePackFontServiceTest()
      : scoped_feature_list_(features::kLanguagePacksFonts),
        testing_prefs_(
            std::make_unique<sync_preferences::TestingPrefServiceSyncable>()) {
    ::RegisterUserProfilePrefs(testing_prefs_->registry());
  }

  void InitProfileWithServices() {
    profile_ =
        TestingProfile::Builder()
            .SetPrefService(std::move(testing_prefs_))
            .AddTestingFactory(
                LanguagePackFontServiceFactory::GetInstance(),
                base::BindRepeating(&LanguagePackFontServiceTest::CreateService,
                                    base::Unretained(this)))
            .Build();
  }

  PrefService* prefs() {
    if (testing_prefs_) {
      CHECK(!profile_);
      return testing_prefs_.get();
    }
    return profile_->GetPrefs();
  }
  FakeDlcserviceClient* dlcservice_client() { return &dlcservice_client_; }
  MockAddFontDir* add_font_dir() { return &add_font_dir_; }

 private:
  std::unique_ptr<KeyedService> CreateService(
      content::BrowserContext* context) {
    return std::make_unique<LanguagePackFontService>(
        Profile::FromBrowserContext(context)->GetPrefs(),
        base::BindRepeating(&MockAddFontDir::Call,
                            base::Unretained(&add_font_dir_)));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  MockAddFontDir add_font_dir_;
  FakeDlcserviceClient dlcservice_client_;
  content::BrowserTaskEnvironment task_environment_;
  // At any point in time, exactly one of the below `unique_ptr`s should be
  // null.
  // On construction, `testing_prefs_` will be created with `profile_` null.
  // After `InitProfile()`, `profile_` will be created by moving in
  // `testing_prefs_`, setting it to null.
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> testing_prefs_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(LanguagePackFontServiceTest, InstallNothingOnUnrelatedLocaleChange) {
  // Ensure that we don't install any DLCs / add any fonts to begin with.
  // Both zz and xx (used below) are not valid ISO 639 locales as of 2024.
  prefs()->SetString(language::prefs::kPreferredLanguages, "zz");

  InitProfileWithServices();
  prefs()->SetString(language::prefs::kPreferredLanguages, "zz,xx");
  base::RunLoop().RunUntilIdle();

  GetExistingDlcsTestFuture future;
  dlcservice_client()->GetExistingDlcs(future.GetCallback());
  const dlcservice::DlcsWithContent& dlcs = future.Get<1>();
  EXPECT_THAT(dlcs.dlc_infos(), IsEmpty());
}

struct ValidFontLanguageTestCase {
  std::string test_name;
  std::string_view preferred_languages_one_locale;
  std::string_view preferred_languages_two_locales;
  std::string dlc_prefix;
  std::string dlc_path;
};

class ValidFontLanguageTest
    : public LanguagePackFontServiceTest,
      public testing::WithParamInterface<ValidFontLanguageTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    LanguagePackFontServiceTest,
    ValidFontLanguageTest,
    testing::ValuesIn<ValidFontLanguageTestCase>(
        {{"Japanese", "zz,ja", "zz,ja,ja-JP", "extrafonts-ja", "/path/for/ja"},
         {"Korean", "zz,ko", "zz,ko,ko-KR", "extrafonts-ko", "/path/for/ko"}}),
    [](const testing::TestParamInfo<ValidFontLanguageTest::ParamType>& info) {
      return info.param.test_name;
    });

TEST_P(ValidFontLanguageTest, InstallValidLanguageOnValidLanguageLocaleChange) {
  const ValidFontLanguageTestCase& test_case = GetParam();

  prefs()->SetString(language::prefs::kPreferredLanguages, "zz");

  InitProfileWithServices();
  prefs()->SetString(language::prefs::kPreferredLanguages,
                     test_case.preferred_languages_one_locale);
  base::RunLoop().RunUntilIdle();

  GetExistingDlcsTestFuture future;
  dlcservice_client()->GetExistingDlcs(future.GetCallback());
  const dlcservice::DlcsWithContent& dlcs = future.Get<1>();
  EXPECT_THAT(dlcs.dlc_infos(),
              ElementsAre(Property(&dlcservice::DlcsWithContent::DlcInfo::id,
                                   StartsWith(test_case.dlc_prefix))));
}

TEST_P(ValidFontLanguageTest,
       InstallValidLanguageOnlyOnceOnMultipleValidLanguageLocalesChange) {
  const ValidFontLanguageTestCase& test_case = GetParam();

  prefs()->SetString(language::prefs::kPreferredLanguages, "zz");

  InitProfileWithServices();
  prefs()->SetString(language::prefs::kPreferredLanguages,
                     test_case.preferred_languages_two_locales);
  base::RunLoop().RunUntilIdle();

  GetExistingDlcsTestFuture future;
  dlcservice_client()->GetExistingDlcs(future.GetCallback());
  const dlcservice::DlcsWithContent& dlcs = future.Get<1>();
  EXPECT_THAT(dlcs.dlc_infos(),
              ElementsAre(Property(&dlcservice::DlcsWithContent::DlcInfo::id,
                                   StartsWith(test_case.dlc_prefix))));
}

TEST_F(LanguagePackFontServiceTest, InstallNothingOnInitWithUnrelatedLocales) {
  {
    dlcservice::DlcState state;
    state.set_state(dlcservice::DlcState::State::DlcState_State_NOT_INSTALLED);
    dlcservice_client()->set_dlc_state(std::move(state));
  }
  prefs()->SetString(language::prefs::kPreferredLanguages, "zz,xx");

  InitProfileWithServices();
  base::RunLoop().RunUntilIdle();

  GetExistingDlcsTestFuture future;
  dlcservice_client()->GetExistingDlcs(future.GetCallback());
  const dlcservice::DlcsWithContent& dlcs = future.Get<1>();
  EXPECT_THAT(dlcs.dlc_infos(), IsEmpty());
}

TEST_P(ValidFontLanguageTest,
       InstallValidLanguageOnInitWithValidLanguageLocale) {
  const ValidFontLanguageTestCase& test_case = GetParam();

  {
    dlcservice::DlcState state;
    state.set_state(dlcservice::DlcState::State::DlcState_State_NOT_INSTALLED);
    dlcservice_client()->set_dlc_state(std::move(state));
  }
  prefs()->SetString(language::prefs::kPreferredLanguages,
                     test_case.preferred_languages_one_locale);

  InitProfileWithServices();
  base::RunLoop().RunUntilIdle();

  GetExistingDlcsTestFuture future;
  dlcservice_client()->GetExistingDlcs(future.GetCallback());
  const dlcservice::DlcsWithContent& dlcs = future.Get<1>();
  EXPECT_THAT(dlcs.dlc_infos(),
              ElementsAre(Property(&dlcservice::DlcsWithContent::DlcInfo::id,
                                   StartsWith(test_case.dlc_prefix))));
}

TEST_P(ValidFontLanguageTest,
       InstallValidLanguageOnlyOnceOnInitWithMultipleValidLanguageLocales) {
  const ValidFontLanguageTestCase& test_case = GetParam();

  {
    dlcservice::DlcState state;
    state.set_state(dlcservice::DlcState::State::DlcState_State_NOT_INSTALLED);
    dlcservice_client()->set_dlc_state(std::move(state));
  }
  prefs()->SetString(language::prefs::kPreferredLanguages,
                     test_case.preferred_languages_two_locales);

  InitProfileWithServices();
  base::RunLoop().RunUntilIdle();

  GetExistingDlcsTestFuture future;
  dlcservice_client()->GetExistingDlcs(future.GetCallback());
  const dlcservice::DlcsWithContent& dlcs = future.Get<1>();
  EXPECT_THAT(dlcs.dlc_infos(),
              ElementsAre(Property(&dlcservice::DlcsWithContent::DlcInfo::id,
                                   StartsWith(test_case.dlc_prefix))));
}

constexpr std::string kUnusedDlcPath = "/path/to/unused/dlc";

TEST_F(LanguagePackFontServiceTest, AddNothingOnUnrelatedLocaleChange) {
  ON_CALL(*add_font_dir(), Call).WillByDefault(Return(true));
  EXPECT_CALL(*add_font_dir(), Call).Times(0);
  {
    dlcservice::DlcState state;
    state.set_state(dlcservice::DlcState::State::DlcState_State_INSTALLED);
    state.set_root_path(kUnusedDlcPath);
    dlcservice_client()->set_dlc_state(std::move(state));
  }
  prefs()->SetString(language::prefs::kPreferredLanguages, "zz");
  InitProfileWithServices();
  prefs()->SetString(language::prefs::kPreferredLanguages, "zz,xx");
  base::RunLoop().RunUntilIdle();
}

TEST_P(ValidFontLanguageTest, AddNothingOnValidLanguageLocaleChange) {
  const ValidFontLanguageTestCase& test_case = GetParam();

  ON_CALL(*add_font_dir(), Call).WillByDefault(Return(true));
  EXPECT_CALL(*add_font_dir(), Call).Times(0);
  {
    dlcservice::DlcState state;
    state.set_state(dlcservice::DlcState::State::DlcState_State_INSTALLED);
    state.set_root_path(test_case.dlc_path);
    dlcservice_client()->set_dlc_state(std::move(state));
  }
  prefs()->SetString(language::prefs::kPreferredLanguages, "zz");
  InitProfileWithServices();
  prefs()->SetString(language::prefs::kPreferredLanguages,
                     test_case.preferred_languages_one_locale);
  base::RunLoop().RunUntilIdle();
}

TEST_F(LanguagePackFontServiceTest, AddNothingOnInitWithUnrelatedLocale) {
  ON_CALL(*add_font_dir(), Call).WillByDefault(Return(true));
  EXPECT_CALL(*add_font_dir(), Call).Times(0);
  {
    dlcservice::DlcState state;
    state.set_state(dlcservice::DlcState::State::DlcState_State_INSTALLED);
    state.set_root_path(kUnusedDlcPath);
    dlcservice_client()->set_dlc_state(std::move(state));
  }
  prefs()->SetString(language::prefs::kPreferredLanguages, "zz,xx");

  InitProfileWithServices();
  base::RunLoop().RunUntilIdle();
}

TEST_P(ValidFontLanguageTest, AddValidLanguageOnInitWithValidLanguageLocale) {
  const ValidFontLanguageTestCase& test_case = GetParam();

  ON_CALL(*add_font_dir(), Call).WillByDefault(Return(true));
  EXPECT_CALL(*add_font_dir(), Call)
      .With(FieldsAre(Property(&base::FilePath::value, test_case.dlc_path)))
      .Times(1);
  {
    dlcservice::DlcState state;
    state.set_state(dlcservice::DlcState::State::DlcState_State_INSTALLED);
    state.set_root_path(test_case.dlc_path);
    dlcservice_client()->set_dlc_state(std::move(state));
  }
  prefs()->SetString(language::prefs::kPreferredLanguages,
                     test_case.preferred_languages_one_locale);

  InitProfileWithServices();
  base::RunLoop().RunUntilIdle();
}

TEST_P(
    ValidFontLanguageTest,
    AddValidLanguageOnInitWithValidLanguageLocaleWhenDownloadedButNotMounted) {
  const ValidFontLanguageTestCase& test_case = GetParam();

  ON_CALL(*add_font_dir(), Call).WillByDefault(Return(true));
  EXPECT_CALL(*add_font_dir(), Call)
      .With(FieldsAre(Property(&base::FilePath::value, test_case.dlc_path)))
      .Times(1);
  {
    dlcservice::DlcState state;
    state.set_id(test_case.dlc_prefix);
    state.set_state(dlcservice::DlcState::State::DlcState_State_NOT_INSTALLED);
    state.set_is_verified(true);
    dlcservice_client()->set_install_root_path(test_case.dlc_path);
    dlcservice_client()->set_dlc_state(std::move(state));
  }
  prefs()->SetString(language::prefs::kPreferredLanguages,
                     test_case.preferred_languages_one_locale);

  InitProfileWithServices();
  base::RunLoop().RunUntilIdle();
}

TEST_P(ValidFontLanguageTest,
       AddValidLanguageOnlyOnceOnInitWithMultipleValidLanguageLocales) {
  const ValidFontLanguageTestCase& test_case = GetParam();

  ON_CALL(*add_font_dir(), Call).WillByDefault(Return(true));
  EXPECT_CALL(*add_font_dir(), Call)
      .With(FieldsAre(Property(&base::FilePath::value, test_case.dlc_path)))
      .Times(1);
  {
    dlcservice::DlcState state;
    state.set_state(dlcservice::DlcState::State::DlcState_State_INSTALLED);
    state.set_root_path(test_case.dlc_path);
    dlcservice_client()->set_dlc_state(std::move(state));
  }
  prefs()->SetString(language::prefs::kPreferredLanguages,
                     test_case.preferred_languages_two_locales);

  InitProfileWithServices();
  base::RunLoop().RunUntilIdle();
}

}  // namespace
}  // namespace ash::language_packs
