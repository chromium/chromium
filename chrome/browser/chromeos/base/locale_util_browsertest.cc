// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/base/locale_util.h"

#include <string>

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "extensions/common/extension_l10n_util.h"

namespace chromeos {
namespace {

void OnLocaleSwitched(base::RunLoop* run_loop,
                      const locale_util::LanguageSwitchResult& result) {
  run_loop->Quit();
}

using LocaleUtilTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(LocaleUtilTest, SwitchLanguage) {
  // Tests default to English.
  EXPECT_EQ("en-US", g_browser_process->GetApplicationLocale());

  // Attempt switch to Belgian French.
  base::RunLoop run_loop;
  locale_util::SwitchLanguage("fr-BE", true, false,
                              base::BindRepeating(&OnLocaleSwitched, &run_loop),
                              ProfileManager::GetActiveUserProfile());
  run_loop.Run();

  // Locale was remapped to generic French.
  EXPECT_EQ("fr", g_browser_process->GetApplicationLocale());

  // Extension subsystem has both actual locale and preferred locale.
  EXPECT_EQ("fr", extension_l10n_util::CurrentLocaleOrDefault());
  EXPECT_EQ("fr-BE", extension_l10n_util::GetPreferredLocaleForTest());
}

}  // namespace
}  // namespace chromeos
