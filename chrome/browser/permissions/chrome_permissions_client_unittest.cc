// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/chrome_permissions_client.h"

#include <memory>

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_request.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromePermissionsClientTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    permissions::PermissionRequestManager::CreateForWebContents(web_contents());
  }
};

#if BUILDFLAG(IS_ANDROID)
TEST_F(ChromePermissionsClientTest,
       MaybeCreateMessageUINeverQuietForUpgradeToPrecise) {
  auto request = std::make_unique<permissions::MockPermissionRequest>(
      GURL(permissions::MockPermissionRequest::kDefaultOrigin),
      permissions::RequestType::kGeolocation,
      permissions::PermissionRequestGestureType::GESTURE,
      permissions::GeolocationPromptType::kUpgradeToPrecise);

  base::WeakPtr<permissions::PermissionPromptAndroid> dummy_prompt;

  auto* client = ChromePermissionsClient::GetInstance();
  auto message_ui =
      client->MaybeCreateMessageUI(web_contents(), *request, dummy_prompt);

  EXPECT_FALSE(message_ui);
}
#endif  // BUILDFLAG(IS_ANDROID)
