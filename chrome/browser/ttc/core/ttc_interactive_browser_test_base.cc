// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/ttc_interactive_browser_test_base.h"

#include <memory>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/app/public/conversation.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/test_utils.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ttc/core/ttc_keyed_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ttc {

class SessionController;

TtcInteractiveBrowserTestBase::TtcInteractiveBrowserTestBase() {
  scoped_feature_list_.InitAndEnableFeature(kTtc);
}

TtcInteractiveBrowserTestBase::~TtcInteractiveBrowserTestBase() = default;

void TtcInteractiveBrowserTestBase::SetUpBrowserContextKeyedServices(
    content::BrowserContext* context) {
  InteractiveBrowserTest::SetUpBrowserContextKeyedServices(context);

  TtcKeyedServiceFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating([](content::BrowserContext* context)
                                       -> std::unique_ptr<KeyedService> {
        return std::make_unique<TtcKeyedService>(
            Profile::FromBrowserContext(context),
            base::BindRepeating([](SessionController&)
                                    -> std::unique_ptr<Conversation> {
              return std::make_unique<testing::NiceMock<MockConversation>>();
            }));
      }));
}

Profile* TtcInteractiveBrowserTestBase::profile() {
  return browser()->GetProfile();
}

TtcKeyedService& TtcInteractiveBrowserTestBase::ttc_service() {
  return CHECK_DEREF(TtcKeyedService::Get(profile()));
}

TtcInteractiveBrowserTestBase::StepBuilder
TtcInteractiveBrowserTestBase::CheckHasSession(bool expected_has_session) {
  return CheckResult(
      [this]() { return ttc_service().session_controller() != nullptr; },
      expected_has_session);
}

}  // namespace ttc
