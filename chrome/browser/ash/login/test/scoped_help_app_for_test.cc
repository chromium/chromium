// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/scoped_help_app_for_test.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/ash/extensions/signin_screen_policy_provider.h"
#include "chrome/browser/ash/login/help_app_launcher.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"

namespace ash {

ScopedHelpAppForTest::ScopedHelpAppForTest() {
  auto reset =
      chromeos::GetScopedSigninScreenPolicyProviderDisablerForTesting();

  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);

  extensions::ChromeTestExtensionLoader loader(
      ProfileHelper::GetSigninProfile());
  loader.set_allow_incognito_access(true);

  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(test_data_dir.AppendASCII("extensions")
                               .AppendASCII("api_test")
                               .AppendASCII("help_app"));

  DCHECK(extension && !extension->id().empty());

  HelpAppLauncher::SetExtensionIdForTest(extension->id().c_str());
}

ScopedHelpAppForTest::~ScopedHelpAppForTest() {
  HelpAppLauncher::SetExtensionIdForTest(nullptr);
}

}  // namespace ash
