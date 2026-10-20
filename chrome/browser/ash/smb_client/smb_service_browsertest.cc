// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smb_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_test.h"

namespace ash {
using SmbServiceTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(SmbServiceTest, IsServiceCreated) {
  const session_manager::Session* primary_session =
      session_manager::SessionManager::Get()->GetPrimarySession();
  ASSERT_TRUE(primary_session);
  content::BrowserContext* browser_context =
      BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          primary_session->account_id());
  EXPECT_TRUE(smb_client::SmbServiceFactory::GetInstance()->IsSmbServiceCreated(
      browser_context));
}

}  // namespace ash
