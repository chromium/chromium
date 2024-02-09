// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/language_packs/language_pack_font_service.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
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

using LanguagePackFontServiceTest = testing::Test;

// `FakeDlcserviceClient::Install` adds DLCs to the return value of
// `GetExistingDlcs`, so we use that to observe whether any DLCs have been
// installed.
using GetExistingDlcsTestFuture =
    base::test::TestFuture<const std::string&,
                           const dlcservice::DlcsWithContent&>;
using MockAddFontDir = testing::MockFunction<bool(base::FilePath)>;

TEST_F(LanguagePackFontServiceTest, InstallNothingOnUnrelatedLocaleChange) {
  FakeDlcserviceClient dlcservice_client;
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();
  MockAddFontDir add_font_dir;
  // Ensure that we don't install any DLCs / add any fonts to begin with.
  // Both zz and xx (used below) are not valid ISO 639 locales as of 2024.
  prefs->SetString(language::prefs::kPreferredLanguages, "zz");

  LanguagePackFontService service(
      prefs, base::BindRepeating(&MockAddFontDir::Call,
                                 base::Unretained(&add_font_dir)));
  prefs->SetString(language::prefs::kPreferredLanguages, "zz,xx");
  base::RunLoop().RunUntilIdle();

  GetExistingDlcsTestFuture future;
  dlcservice_client.GetExistingDlcs(future.GetCallback());
  const dlcservice::DlcsWithContent& dlcs = future.Get<1>();
  EXPECT_THAT(dlcs.dlc_infos(), IsEmpty());
}

TEST_F(LanguagePackFontServiceTest, InstallJapaneseOnJapaneseLocaleChange) {
  FakeDlcserviceClient dlcservice_client;
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();
  MockAddFontDir add_font_dir;
  prefs->SetString(language::prefs::kPreferredLanguages, "zz");

  LanguagePackFontService service(
      prefs, base::BindRepeating(&MockAddFontDir::Call,
                                 base::Unretained(&add_font_dir)));
  prefs->SetString(language::prefs::kPreferredLanguages, "zz,ja");
  base::RunLoop().RunUntilIdle();

  GetExistingDlcsTestFuture future;
  dlcservice_client.GetExistingDlcs(future.GetCallback());
  const dlcservice::DlcsWithContent& dlcs = future.Get<1>();
  EXPECT_THAT(dlcs.dlc_infos(),
              ElementsAre(Property(&dlcservice::DlcsWithContent::DlcInfo::id,
                                   StartsWith("extrafonts-ja"))));
}

TEST_F(LanguagePackFontServiceTest,
       InstallJapaneseOnlyOnceOnMultipleJapaneseLocaleChange) {
  FakeDlcserviceClient dlcservice_client;
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();
  MockAddFontDir add_font_dir;
  prefs->SetString(language::prefs::kPreferredLanguages, "zz");

  LanguagePackFontService service(
      prefs, base::BindRepeating(&MockAddFontDir::Call,
                                 base::Unretained(&add_font_dir)));
  prefs->SetString(language::prefs::kPreferredLanguages, "zz,ja,ja-JP");
  base::RunLoop().RunUntilIdle();

  GetExistingDlcsTestFuture future;
  dlcservice_client.GetExistingDlcs(future.GetCallback());
  const dlcservice::DlcsWithContent& dlcs = future.Get<1>();
  EXPECT_THAT(dlcs.dlc_infos(),
              ElementsAre(Property(&dlcservice::DlcsWithContent::DlcInfo::id,
                                   StartsWith("extrafonts-ja"))));
}

TEST_F(LanguagePackFontServiceTest, InstallNothingOnInitWithUnrelatedLocales) {
  FakeDlcserviceClient dlcservice_client;
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();
  MockAddFontDir add_font_dir;
  {
    dlcservice::DlcState state;
    state.set_state(dlcservice::DlcState::State::DlcState_State_NOT_INSTALLED);
    dlcservice_client.set_dlc_state(std::move(state));
  }
  prefs->SetString(language::prefs::kPreferredLanguages, "zz,xx");

  LanguagePackFontService service(
      prefs, base::BindRepeating(&MockAddFontDir::Call,
                                 base::Unretained(&add_font_dir)));
  base::RunLoop().RunUntilIdle();

  GetExistingDlcsTestFuture future;
  dlcservice_client.GetExistingDlcs(future.GetCallback());
  const dlcservice::DlcsWithContent& dlcs = future.Get<1>();
  EXPECT_THAT(dlcs.dlc_infos(), IsEmpty());
}

TEST_F(LanguagePackFontServiceTest, InstallJapaneseOnInitWithJapaneseLocale) {
  FakeDlcserviceClient dlcservice_client;
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();
  MockAddFontDir add_font_dir;
  {
    dlcservice::DlcState state;
    state.set_state(dlcservice::DlcState::State::DlcState_State_NOT_INSTALLED);
    dlcservice_client.set_dlc_state(std::move(state));
  }
  prefs->SetString(language::prefs::kPreferredLanguages, "zz,ja");

  LanguagePackFontService service(
      prefs, base::BindRepeating(&MockAddFontDir::Call,
                                 base::Unretained(&add_font_dir)));
  base::RunLoop().RunUntilIdle();

  GetExistingDlcsTestFuture future;
  dlcservice_client.GetExistingDlcs(future.GetCallback());
  const dlcservice::DlcsWithContent& dlcs = future.Get<1>();
  EXPECT_THAT(dlcs.dlc_infos(),
              ElementsAre(Property(&dlcservice::DlcsWithContent::DlcInfo::id,
                                   StartsWith("extrafonts-ja"))));
}

TEST_F(LanguagePackFontServiceTest,
       InstallJapaneseOnlyOnceOnInitWithMultipleJapaneseLocales) {
  FakeDlcserviceClient dlcservice_client;
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();
  MockAddFontDir add_font_dir;
  {
    dlcservice::DlcState state;
    state.set_state(dlcservice::DlcState::State::DlcState_State_NOT_INSTALLED);
    dlcservice_client.set_dlc_state(std::move(state));
  }
  prefs->SetString(language::prefs::kPreferredLanguages, "zz,ja,ja-JP");

  LanguagePackFontService service(
      prefs, base::BindRepeating(&MockAddFontDir::Call,
                                 base::Unretained(&add_font_dir)));
  base::RunLoop().RunUntilIdle();

  GetExistingDlcsTestFuture future;
  dlcservice_client.GetExistingDlcs(future.GetCallback());
  const dlcservice::DlcsWithContent& dlcs = future.Get<1>();
  EXPECT_THAT(dlcs.dlc_infos(),
              ElementsAre(Property(&dlcservice::DlcsWithContent::DlcInfo::id,
                                   StartsWith("extrafonts-ja"))));
}

constexpr std::string kJapaneseFontPath = "/path/to/ja";

TEST_F(LanguagePackFontServiceTest, AddNothingOnUnrelatedLocaleChange) {
  FakeDlcserviceClient dlcservice_client;
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();
  MockAddFontDir add_font_dir;
  ON_CALL(add_font_dir, Call).WillByDefault(Return(true));
  EXPECT_CALL(add_font_dir, Call).Times(0);
  {
    dlcservice::DlcState state;
    state.set_state(dlcservice::DlcState::State::DlcState_State_INSTALLED);
    state.set_root_path(kJapaneseFontPath);
    dlcservice_client.set_dlc_state(std::move(state));
  }
  prefs->SetString(language::prefs::kPreferredLanguages, "zz");
  LanguagePackFontService service(
      prefs, base::BindRepeating(&MockAddFontDir::Call,
                                 base::Unretained(&add_font_dir)));
  prefs->SetString(language::prefs::kPreferredLanguages, "zz,xx");
  base::RunLoop().RunUntilIdle();
}

TEST_F(LanguagePackFontServiceTest, AddNothingOnJapaneseLocaleChange) {
  FakeDlcserviceClient dlcservice_client;
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();
  MockAddFontDir add_font_dir;
  ON_CALL(add_font_dir, Call).WillByDefault(Return(true));
  EXPECT_CALL(add_font_dir, Call).Times(0);
  {
    dlcservice::DlcState state;
    state.set_state(dlcservice::DlcState::State::DlcState_State_INSTALLED);
    state.set_root_path(kJapaneseFontPath);
    dlcservice_client.set_dlc_state(std::move(state));
  }
  prefs->SetString(language::prefs::kPreferredLanguages, "zz");
  LanguagePackFontService service(
      prefs, base::BindRepeating(&MockAddFontDir::Call,
                                 base::Unretained(&add_font_dir)));
  prefs->SetString(language::prefs::kPreferredLanguages, "zz,ja");
  base::RunLoop().RunUntilIdle();
}

TEST_F(LanguagePackFontServiceTest, AddNothingOnInitWithUnrelatedLocale) {
  FakeDlcserviceClient dlcservice_client;
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();
  MockAddFontDir add_font_dir;
  ON_CALL(add_font_dir, Call).WillByDefault(Return(true));
  EXPECT_CALL(add_font_dir, Call).Times(0);
  {
    dlcservice::DlcState state;
    state.set_state(dlcservice::DlcState::State::DlcState_State_INSTALLED);
    state.set_root_path(kJapaneseFontPath);
    dlcservice_client.set_dlc_state(std::move(state));
  }
  prefs->SetString(language::prefs::kPreferredLanguages, "zz,xx");

  LanguagePackFontService service(
      prefs, base::BindRepeating(&MockAddFontDir::Call,
                                 base::Unretained(&add_font_dir)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(LanguagePackFontServiceTest, AddJapaneseOnInitWithJapaneseLocale) {
  FakeDlcserviceClient dlcservice_client;
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();
  MockAddFontDir add_font_dir;
  ON_CALL(add_font_dir, Call).WillByDefault(Return(true));
  EXPECT_CALL(add_font_dir, Call)
      .With(FieldsAre(Property(&base::FilePath::value, kJapaneseFontPath)))
      .Times(1);
  {
    dlcservice::DlcState state;
    state.set_state(dlcservice::DlcState::State::DlcState_State_INSTALLED);
    state.set_root_path(kJapaneseFontPath);
    dlcservice_client.set_dlc_state(std::move(state));
  }
  prefs->SetString(language::prefs::kPreferredLanguages, "zz,ja");

  LanguagePackFontService service(
      prefs, base::BindRepeating(&MockAddFontDir::Call,
                                 base::Unretained(&add_font_dir)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(LanguagePackFontServiceTest,
       AddJapaneseOnlyOnceOnInitWithMultipleJapaneseLocales) {
  FakeDlcserviceClient dlcservice_client;
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();
  MockAddFontDir add_font_dir;
  ON_CALL(add_font_dir, Call).WillByDefault(Return(true));
  EXPECT_CALL(add_font_dir, Call)
      .With(FieldsAre(Property(&base::FilePath::value, kJapaneseFontPath)))
      .Times(1);
  {
    dlcservice::DlcState state;
    state.set_state(dlcservice::DlcState::State::DlcState_State_INSTALLED);
    state.set_root_path(kJapaneseFontPath);
    dlcservice_client.set_dlc_state(std::move(state));
  }
  prefs->SetString(language::prefs::kPreferredLanguages, "zz,ja,ja-JP");

  LanguagePackFontService service(
      prefs, base::BindRepeating(&MockAddFontDir::Call,
                                 base::Unretained(&add_font_dir)));
  base::RunLoop().RunUntilIdle();
}

}  // namespace
}  // namespace ash::language_packs
