// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/browser_prefs.h"

#include <cstddef>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/chrome_browser_main_extra_parts_profiles.h"
#include "chrome/browser/profiles/pref_service_builder_utils.h"
#include "components/sync/base/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BrowserPrefsTest : public testing::Test {
 protected:
  BrowserPrefsTest() {
    // Invoking `RegisterUserProfilePrefs()` isn't sufficient as some migration
    // code relies on prefs registered by the keyed service factories, via
    // BrowserContextKeyedService::RegisterProfilePrefs().
    ChromeBrowserMainExtraPartsProfiles::
        EnsureBrowserContextKeyedServiceFactoriesBuilt();
    RegisterProfilePrefs(/*is_signin_profile=*/false,
                         g_browser_process->GetApplicationLocale(),
                         prefs_.registry());
  }

  // The task environment is needed because some keyed services CHECK for things
  // like content::BrowserThread::CurrentlyOn(content::BrowserThread::UI).
  content::BrowserTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
};

}  // namespace

#if BUILDFLAG(IS_ANDROID)
TEST_F(BrowserPrefsTest, DeletesObsoleteLoginDataFilesWithoutBlocking) {
  base::ScopedTempDir profile_dir;
  ASSERT_TRUE(profile_dir.CreateUniqueTempDir());
  const base::FilePath login_data_files[] = {
      profile_dir.GetPath().Append(FILE_PATH_LITERAL("Login Data")),
      profile_dir.GetPath().Append(FILE_PATH_LITERAL("Login Data For Account")),
      profile_dir.GetPath().Append(FILE_PATH_LITERAL("Login Data-journal")),
      profile_dir.GetPath().Append(
          FILE_PATH_LITERAL("Login Data For Account-journal")),
  };
  for (const base::FilePath& file : login_data_files) {
    ASSERT_TRUE(base::WriteFile(file, ""));
  }

  {
    // Profiles can be loaded after blocking is disallowed on the UI thread.
    base::ScopedDisallowBlocking disallow_blocking;
    MigrateObsoleteProfilePrefs(&prefs_, profile_dir.GetPath());
  }
  // The files are deleted on the thread pool, which doesn't wake up the main
  // thread, so wait for the thread pool directly.
  base::ThreadPoolInstance::Get()->FlushForTesting();

  for (const base::FilePath& file : login_data_files) {
    EXPECT_FALSE(base::PathExists(file)) << file;
  }
}
#endif  // BUILDFLAG(IS_ANDROID)
