// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/base/locale_util.h"

#include <string>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_l10n_util.h"

namespace ash {
namespace {

void OnLocaleSwitched(base::RunLoop* run_loop,
                      const locale_util::LanguageSwitchResult& result) {
  run_loop->Quit();
}

using LocaleUtilTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(LocaleUtilTest, SwitchLanguage) {
  ApplicationLocaleStorage* application_locale_storage =
      g_browser_process->GetFeatures()->application_locale_storage();

  // Tests default to English.
  EXPECT_EQ("en-US", application_locale_storage->Get());

  // Attempt switch to Belgian French.
  base::RunLoop run_loop;
  locale_util::SwitchLanguage(application_locale_storage, "fr-BE", true, false,
                              base::BindRepeating(&OnLocaleSwitched, &run_loop),
                              ProfileManager::GetActiveUserProfile());
  run_loop.Run();

  // Locale was remapped to generic French.
  EXPECT_EQ("fr", application_locale_storage->Get());

  // Extension subsystem has both actual locale and preferred locale.
  EXPECT_EQ("fr", extension_l10n_util::CurrentLocaleOrDefault());
  EXPECT_EQ("fr-BE", extension_l10n_util::GetPreferredLocaleForTest());
}

}  // namespace
}  // namespace ash
