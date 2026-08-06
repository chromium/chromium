// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_coordinator.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "ui/gfx/geometry/rect.h"
#endif

namespace glic {

#if !BUILDFLAG(IS_ANDROID)
using GlicExperimentalTriggeringBrowserTest = GlicBrowserTest;

IN_PROC_BROWSER_TEST_F(
    GlicExperimentalTriggeringBrowserTest,
    ExperimentalTriggeringWithFocusedPwaDoesNotCreateNewWindow) {
  GlicEnabling::SetBypassEnablementChecksForTesting(true);
  auto* glic_service = GlicKeyedServiceFactory::GetGlicKeyedService(
      GetProfile(), /*create=*/true);
  ASSERT_TRUE(glic_service);
  glic_service->enabling().SetCompletedFre(glic::prefs::FreStatus::kCompleted);
  glic_service->enabling().SetUserEnabledActuationOnWeb(true);
  glic_service->enabling().SetExperimentalTriggeringEnabled(true);

  // 1. Create and show a PWA app window.
  Browser* app_browser =
      CreateBrowserWindow(BrowserWindowCreateParams::CreateForApp(
                              "test_app", /*trusted_source=*/true, gfx::Rect(),
                              GetProfile(), /*user_gesture=*/true))
          ->GetBrowserForMigrationOnly();
  app_browser->GetWindow()->Show();

  // 2. Experimental triggering request fires while PWA window is focused.
  GlicExperimentalTriggeringCoordinator coordinator(GetProfile());
  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = "test_context";
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "hello"};

  coordinator.OnRequest(
      "test_context", request,
      ScopedIncomingMessageResultLogger(
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      base::DoNothing(), nullptr);

  // 3. Verify no new normal browser window was created.
  size_t normal_browser_count = 0;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&](BrowserWindowInterface* b) {
        if (b->GetProfile() == GetProfile() &&
            b->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL) {
          normal_browser_count++;
        }
        return true;
      });
  EXPECT_EQ(1u, normal_browser_count);

  CloseBrowserSynchronously(app_browser);
}
#endif

}  // namespace glic
