// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/media_app/mahi_media_app_client.h"

#include <cstdint>

#include "ash/shell.h"
#include "ash/system/mahi/test/mock_mahi_media_app_content_manager.h"
#include "ash/system/mahi/test/mock_mahi_media_app_events_proxy.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_window_builder.h"
#include "ash/webui/media_app_ui/media_app_ui_untrusted.mojom.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_content_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_events_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"

namespace ash {

namespace {
using ::testing::_;
using ::testing::InSequence;
using ::testing::MockFunction;
}  // namespace

class MockMahiUntrustedPage
    : public ash::media_app_ui::mojom::MahiUntrustedPage {
 public:
  MockMahiUntrustedPage() = default;
  ~MockMahiUntrustedPage() override = default;

  MOCK_METHOD(void, HidePdfContextMenu, (), (override));

  MOCK_METHOD(void,
              GetPdfContent,
              (int32_t, GetPdfContentCallback),
              (override));
};

class MahiMediaAppClientTest : public AshTestBase {
 public:
  MahiMediaAppClientTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  MahiMediaAppClientTest(const MahiMediaAppClientTest&) = delete;
  MahiMediaAppClientTest& operator=(const MahiMediaAppClientTest&) = delete;

  ~MahiMediaAppClientTest() override = default;

  void SetUp() override {
    // On MahiMediaAppClient destruction, it notifies an `OnPdfClosed` event.
    EXPECT_CALL(mock_mahi_media_app_events_proxy_, OnPdfClosed(_)).Times(1);
    AshTestBase::SetUp();
  }

 protected:
  testing::NiceMock<MockMahiMediaAppContentManager>
      mock_mahi_media_app_content_manager_;
  chromeos::ScopedMahiMediaAppContentManagerSetter
      scoped_mahi_media_app_content_manager_{
          &mock_mahi_media_app_content_manager_};

  testing::NiceMock<MockMahiMediaAppEventsProxy>
      mock_mahi_media_app_events_proxy_;
  chromeos::ScopedMahiMediaAppEventsProxySetter
      scoped_mahi_media_app_events_proxy_{&mock_mahi_media_app_events_proxy_};

  testing::StrictMock<MockMahiUntrustedPage> mahi_untrusted_page_;
  mojo::Receiver<ash::media_app_ui::mojom::MahiUntrustedPage> receiver_{
      &mahi_untrusted_page_};
};

// Tests that requests to media app can be forwarded via mojo::Remote.
TEST_F(MahiMediaAppClientTest, HideMediaAppContextMenu) {
  std::unique_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(-1, nullptr));

  auto mahi_media_app_client_ = std::make_unique<MahiMediaAppClient>(
      receiver_.BindNewPipeAndPassRemote(), "test_name", window.get());

  EXPECT_CALL(mahi_untrusted_page_, HidePdfContextMenu()).Times(1);

  mahi_media_app_client_->HideMediaAppContextMenu();
}

// Tests that `GetPdfContent` handles word count properly.
// It tries to get text content from PDF file that is no more than 5,000,000
// bytes, and consider content valid when its word count >= 50.
TEST_F(MahiMediaAppClientTest, GetPdfContent) {
  std::unique_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(-1, nullptr));

  auto mahi_media_app_client_ = std::make_unique<MahiMediaAppClient>(
      receiver_.BindNewPipeAndPassRemote(), "test_name", window.get());

  {
    // PDF content doesn't meet word count.
    EXPECT_CALL(mahi_untrusted_page_, GetPdfContent)
        .WillOnce([](int32_t limit,
                     MockMahiUntrustedPage::GetPdfContentCallback callback) {
          EXPECT_EQ(limit, 5 * 1000 * 1000);
          std::move(callback).Run("abc");
        });

    mahi_media_app_client_->GetPdfContent(
        base::BindOnce([](crosapi::mojom::MahiPageContentPtr mahi_content_ptr) {
          EXPECT_TRUE(mahi_content_ptr.is_null());
        }));
    base::RunLoop().RunUntilIdle();
  }

  {
    // PDF content word count is enough.
    EXPECT_CALL(mahi_untrusted_page_, GetPdfContent)
        .WillOnce([](int32_t limit,
                     MockMahiUntrustedPage::GetPdfContentCallback callback) {
          EXPECT_EQ(limit, 5 * 1000 * 1000);
          std::vector<std::string> parts(100, "abc");
          std::move(callback).Run(base::JoinString(parts, " "));
        });

    mahi_media_app_client_->GetPdfContent(
        base::BindOnce([](crosapi::mojom::MahiPageContentPtr mahi_content_ptr) {
          EXPECT_FALSE(mahi_content_ptr.is_null());
          EXPECT_EQ(mahi_content_ptr->page_content,
                    base::UTF8ToUTF16(base::JoinString(
                        std::vector<std::string>(100, "abc"), " ")));
        }));
    base::RunLoop().RunUntilIdle();
  }
}

// Tests that MahiMediaAppClient resets its `media_app_window_` when it's
// destroying.
TEST_F(MahiMediaAppClientTest, WindowDestroying) {
  std::unique_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(-1, nullptr));

  auto mahi_media_app_client_ = std::make_unique<MahiMediaAppClient>(
      receiver_.BindNewPipeAndPassRemote(), "test_name", window.get());

  EXPECT_EQ(mahi_media_app_client_->media_app_window(), window.get());

  window.reset();
  EXPECT_EQ(mahi_media_app_client_->media_app_window(), nullptr);
}

// Tests the client can observe the focus events and notify proxy when the
// associated media app window gets focus.
TEST_F(MahiMediaAppClientTest, WindowFocus) {
  aura::test::TestWindowDelegate wd1;
  wd1.set_can_focus(true);
  std::unique_ptr<aura::Window> window_1(
      CreateTestWindowInShellWithDelegate(&wd1, -1, gfx::Rect(10, 10, 50, 50)));
  aura::test::TestWindowDelegate wd2;
  wd2.set_can_focus(true);
  std::unique_ptr<aura::Window> window_2(
      CreateTestWindowInShellWithDelegate(&wd2, -2, gfx::Rect(70, 70, 50, 50)));

  // `window_2` has focus.
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(window_2.get());

  // Creates a client with media_app_window_ = window_1.
  auto mahi_media_app_client_ = std::make_unique<MahiMediaAppClient>(
      receiver_.BindNewPipeAndPassRemote(), "test_name", window_1.get());

  MockFunction<void(std::string check_point_name)> check;
  {
    InSequence sequence;
    EXPECT_CALL(check, Call("PDF loaded"));
    EXPECT_CALL(mock_mahi_media_app_events_proxy_, OnPdfGetFocus(_)).Times(1);
    EXPECT_CALL(check, Call("focus window_2"));
    EXPECT_CALL(check, Call("focus window_1"));
    EXPECT_CALL(mock_mahi_media_app_events_proxy_, OnPdfGetFocus(_)).Times(1);
  }

  // `window_1` gets focus, but because `OnPdfLoaded` is not called, the client
  // doesn't observe this focus event.
  focus_client->FocusWindow(window_1.get());

  // `OnPdfLoaded` called, the client starts to observer focus event. And
  // because `window_1` is currently focused, it notifies events proxy
  // instantly.
  check.Call("PDF loaded");
  mahi_media_app_client_->OnPdfLoaded();

  // Switches focus to `window_2` and back to `window_1`, the client notifies
  // event proxy again.
  check.Call("focus window_2");
  focus_client->FocusWindow(window_2.get());
  check.Call("focus window_1");
  focus_client->FocusWindow(window_1.get());
}

// Tests the client can observe the focus events and notify proxy when the
// associated media app window gets focus.
TEST_F(MahiMediaAppClientTest, PdfRename) {
  aura::test::TestWindowDelegate wd;
  wd.set_can_focus(true);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithDelegate(&wd, -1, gfx::Rect(10, 10, 50, 50)));

  // `window` has focus.
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(window.get());

  MockFunction<void(std::string check_point_name)> check;
  {
    InSequence sequence;
    EXPECT_CALL(check, Call("PDF loaded"));
    EXPECT_CALL(mock_mahi_media_app_events_proxy_, OnPdfGetFocus(_)).Times(1);
    EXPECT_CALL(check, Call("updated with same name"));
    EXPECT_CALL(check, Call("updated with new name"));
    EXPECT_CALL(mock_mahi_media_app_events_proxy_, OnPdfGetFocus(_)).Times(1);
  }

  // Creates a client.
  auto mahi_media_app_client_ = std::make_unique<MahiMediaAppClient>(
      receiver_.BindNewPipeAndPassRemote(), "test_name", window.get());

  check.Call("PDF loaded");
  mahi_media_app_client_->OnPdfLoaded();

  check.Call("updated with same name");
  mahi_media_app_client_->OnPdfFileNameUpdated("test_name");
  check.Call("updated with new name");
  mahi_media_app_client_->OnPdfFileNameUpdated("new_name");

  EXPECT_EQ(mahi_media_app_client_->file_name(), "new_name");
}
}  // namespace ash
