// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/language_packs/language_pack_font_service.h"

#include <string>

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
using testing::IsEmpty;
using testing::Property;
using testing::StartsWith;

using LanguagePackFontServiceTest = testing::Test;

// `FakeDlcserviceClient::Install` adds DLCs to the return value of
// `GetExistingDlcs`, so we use that to observe whether any DLCs have been
// installed.
using GetExistingDlcsTestFuture =
    base::test::TestFuture<const std::string&,
                           const dlcservice::DlcsWithContent&>;

TEST_F(LanguagePackFontServiceTest, InstallNothingOnUnrelatedLocaleChange) {
  FakeDlcserviceClient dlcservice_client;
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();

  LanguagePackFontService service(prefs);
  // Both zz and xx are not valid ISO 639 locales as of 2024.
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

  LanguagePackFontService service(prefs);
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

  LanguagePackFontService service(prefs);
  prefs->SetString(language::prefs::kPreferredLanguages, "zz,ja,ja-JP");
  base::RunLoop().RunUntilIdle();

  GetExistingDlcsTestFuture future;
  dlcservice_client.GetExistingDlcs(future.GetCallback());
  const dlcservice::DlcsWithContent& dlcs = future.Get<1>();
  EXPECT_THAT(dlcs.dlc_infos(),
              ElementsAre(Property(&dlcservice::DlcsWithContent::DlcInfo::id,
                                   StartsWith("extrafonts-ja"))));
}

}  // namespace
}  // namespace ash::language_packs
