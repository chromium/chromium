// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/service/accessibility_service_devtools_delegate.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/test/browser_test.h"

namespace ash {

class AccessibilityServiceDevToolsDelegateTest : public InProcessBrowserTest {
 public:
  AccessibilityServiceDevToolsDelegateTest() = default;
  AccessibilityServiceDevToolsDelegateTest(
      const AccessibilityServiceDevToolsDelegateTest&) = delete;
  AccessibilityServiceDevToolsDelegateTest& operator=(
      const AccessibilityServiceDevToolsDelegateTest&) = delete;
  ~AccessibilityServiceDevToolsDelegateTest() override = default;

  AccessibilityServiceDevToolsDelegate::ConnectDevToolsAgentCallback
  CreateTestDevToolsAgentCallback() {
    return base::BindRepeating(
        &AccessibilityServiceDevToolsDelegateTest::IncrementCallbackCalls,
        base::Unretained(this));
  }

  void IncrementCallbackCalls(mojo::PendingAssociatedReceiver<
                                  blink::mojom::DevToolsAgent> agent_receiver,
                              ax::mojom::AssistiveTechnologyType type) {
    callback_calls++;
  }

 protected:
  mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent;
  int callback_calls = 0;
};

IN_PROC_BROWSER_TEST_F(AccessibilityServiceDevToolsDelegateTest,
                       ConnectDevToolsAgentCallback) {
  auto callback = CreateTestDevToolsAgentCallback();
  auto delegate = AccessibilityServiceDevToolsDelegate(
      ax::mojom::AssistiveTechnologyType::kChromeVox, callback);
  delegate.ConnectDevToolsAgent(std::move(agent));

  EXPECT_EQ(callback_calls, 1);
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceDevToolsDelegateTest, NoOpTests) {
  auto callback = CreateTestDevToolsAgentCallback();
  auto delegate = AccessibilityServiceDevToolsDelegate(
      ax::mojom::AssistiveTechnologyType::kChromeVox, callback);

  EXPECT_FALSE(delegate.Activate());
  EXPECT_FALSE(delegate.Close());
  EXPECT_FALSE(delegate.ForceIOSession());
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceDevToolsDelegateTest,
                       HostReturnsDelegateValues) {
  auto callback = CreateTestDevToolsAgentCallback();
  auto delegate_ptr = std::make_unique<AccessibilityServiceDevToolsDelegate>(
      ax::mojom::AssistiveTechnologyType::kChromeVox, callback);
  // Keep a reference to the pointer.
  auto* delegate = delegate_ptr.get();

  auto host = content::DevToolsAgentHost::CreateForMojomDelegate(
      "id", std::move(delegate_ptr));
  EXPECT_EQ(delegate->GetType(), host->GetType());
  EXPECT_EQ(delegate->GetTitle(), host->GetTitle());
  EXPECT_EQ(delegate->GetURL(), host->GetURL());
  EXPECT_EQ(delegate->Activate(), host->Activate());
  EXPECT_EQ(delegate->Close(), host->Close());
}
}  // namespace ash
