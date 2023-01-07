// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/token.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/message_center.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/notification.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace crosapi {
namespace {

using ::testing::Contains;
using ::testing::Not;

// Creates a simple notification with |id|.
mojom::NotificationPtr CreateNotificationWithId(const std::string& id) {
  auto notification = mojom::Notification::New();
  notification->type = mojom::NotificationType::kSimple;
  notification->id = id;
  return notification;
}

class TestDelegate : public mojom::NotificationDelegate {
 public:
  TestDelegate() = default;
  TestDelegate(const TestDelegate&) = delete;
  TestDelegate& operator=(const TestDelegate&) = delete;
  ~TestDelegate() override = default;

  // crosapi::mojom::NotificationDelegate:
  void OnNotificationClosed(bool by_user) override {
    if (on_closed_run_loop_)
      on_closed_run_loop_->Quit();
  }
  void OnNotificationClicked() override {}
  void OnNotificationButtonClicked(
      uint32_t button_index,
      const absl::optional<std::u16string>& reply) override {}
  void OnNotificationSettingsButtonClicked() override {}
  void OnNotificationDisabled() override {}

  // Public because this is test code.
  base::RunLoop* on_closed_run_loop_ = nullptr;
  mojo::Receiver<mojom::NotificationDelegate> receiver_{this};
};

class MessageCenterLacrosBrowserTest : public InProcessBrowserTest {
 public:
  MessageCenterLacrosBrowserTest() = default;
  MessageCenterLacrosBrowserTest(const MessageCenterLacrosBrowserTest&) =
      delete;
  MessageCenterLacrosBrowserTest& operator=(
      const MessageCenterLacrosBrowserTest&) = delete;
  ~MessageCenterLacrosBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(MessageCenterLacrosBrowserTest, Basics) {
  auto& remote = chromeos::LacrosService::Get()
                     ->GetRemote<crosapi::mojom::MessageCenter>();
  ASSERT_TRUE(remote.get());

  // Display some notifications. Use cryptographically random IDs so they won't
  // collide with existing system notifications or other test notifications.
  TestDelegate delegate1;
  std::string id1 = base::Token::CreateRandom().ToString();
  remote->DisplayNotification(CreateNotificationWithId(id1),
                              delegate1.receiver_.BindNewPipeAndPassRemote());
  TestDelegate delegate2;
  std::string id2 = base::Token::CreateRandom().ToString();
  remote->DisplayNotification(CreateNotificationWithId(id2),
                              delegate2.receiver_.BindNewPipeAndPassRemote());

  // Read back the displayed notifications.
  std::vector<std::string> ids;
  mojom::MessageCenterAsyncWaiter waiter(remote.get());
  waiter.GetDisplayedNotifications(&ids);
  EXPECT_THAT(ids, Contains(id1));
  EXPECT_THAT(ids, Contains(id2));

  // Close notification 1. The delegate should be notified.
  base::RunLoop run_loop1;
  delegate1.on_closed_run_loop_ = &run_loop1;
  remote->CloseNotification(id1);
  run_loop1.Run();

  // Notification 1 is gone but notification 2 remains.
  waiter.GetDisplayedNotifications(&ids);
  EXPECT_THAT(ids, Not(Contains(id1)));
  EXPECT_THAT(ids, Contains(id2));

  // Close notification 2. The delegate should be notified.
  base::RunLoop run_loop2;
  delegate2.on_closed_run_loop_ = &run_loop2;
  remote->CloseNotification(id2);
  run_loop2.Run();

  // Both notifications are gone.
  waiter.GetDisplayedNotifications(&ids);
  EXPECT_THAT(ids, Not(Contains(id1)));
  EXPECT_THAT(ids, Not(Contains(id2)));
}

}  // namespace
}  // namespace crosapi
