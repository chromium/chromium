// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smb_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"

namespace ash {
using SmbServiceTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(SmbServiceTest, IsServiceCreated) {
  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  content::BrowserContext* browser_context =
      BrowserContextHelper::Get()->GetBrowserContextByUser(primary_user);
  EXPECT_TRUE(smb_client::SmbServiceFactory::GetInstance()->IsSmbServiceCreated(
      browser_context));
}

}  // namespace ash
