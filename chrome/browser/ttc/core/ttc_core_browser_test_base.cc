// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/ttc_core_browser_test_base.h"

#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/core/test_utils.h"
#include "chrome/browser/ttc/features.h"
#include "chrome/browser/ttc/session_controller_impl.h"
#include "chrome/browser/ttc/ttc_keyed_service.h"
#include "chrome/browser/ttc/ttc_keyed_service_factory.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ttc {

TtcCoreBrowserTestBase::TtcCoreBrowserTestBase() {
  scoped_feature_list_.InitAndEnableFeature(kTtc);
}

TtcCoreBrowserTestBase::~TtcCoreBrowserTestBase() = default;

void TtcCoreBrowserTestBase::SetUpOnMainThread() {
  PlatformBrowserTest::SetUpOnMainThread();
  embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  // Replace the service with one whose sessions use a MockConversation.
  TtcKeyedServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
        return std::make_unique<TtcKeyedService>(
            Profile::FromBrowserContext(context),
            base::BindRepeating(&TtcCoreBrowserTestBase::MakeMockConversation));
      }));
}

Profile* TtcCoreBrowserTestBase::profile() {
  return chrome_test_utils::GetProfile(this);
}

content::WebContents* TtcCoreBrowserTestBase::web_contents() {
  return chrome_test_utils::GetActiveWebContents(this);
}

TtcKeyedService& TtcCoreBrowserTestBase::ttc_service() {
  return CHECK_DEREF(TtcKeyedService::Get(profile()));
}

MockConversation* TtcCoreBrowserTestBase::conversation() {
  auto* session_controller =
      static_cast<SessionControllerImpl*>(ttc_service().session_controller());
  if (!session_controller) {
    return nullptr;
  }
  return static_cast<MockConversation*>(&session_controller->conversation());
}

// static
std::unique_ptr<Conversation> TtcCoreBrowserTestBase::MakeMockConversation(
    Profile*) {
  return std::make_unique<testing::NiceMock<MockConversation>>();
}

}  // namespace ttc
