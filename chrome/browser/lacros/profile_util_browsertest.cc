// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/files/file_path.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lacros/profile_util.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

using ProfileUtilBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(ProfileUtilBrowserTest, HashProfilePathToProfileId) {
  base::FilePath filepath("/home/chronos/Default");
  EXPECT_EQ(HashProfilePathToProfileId(filepath), 11368151317684173739u);
}

IN_PROC_BROWSER_TEST_F(ProfileUtilBrowserTest,
                       GetProfileAttributesWithProfileId) {
  ProfileAttributesEntry* non_exist_entry =
      GetProfileAttributesWithProfileId(0);
  EXPECT_FALSE(non_exist_entry);

  std::vector<ProfileAttributesEntry*> entries =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetAllProfilesAttributesSortedByNameWithCheck();
  for (auto* entry : entries) {
    const uint64_t profile_id = HashProfilePathToProfileId(entry->GetPath());
    EXPECT_EQ(entry, GetProfileAttributesWithProfileId(profile_id));
  }
}
