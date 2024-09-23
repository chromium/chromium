// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/clipboard_history_ash.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/clipboard/test_support/mock_clipboard_history_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom-shared.h"
#include "chromeos/ui/clipboard_history/clipboard_history_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/events/event_constants.h"

namespace crosapi {

namespace {

using ::base::test::InvokeFuture;
using ::testing::ElementsAre;

// Matchers --------------------------------------------------------------------

MATCHER_P2(DescriptorMatches, display_format, display_text, "") {
  return arg->display_format == display_format &&
         arg->display_text == display_text;
}

// Helper classes --------------------------------------------------------------

// A mocked client to check the descriptors received from Ash.
class MockedClipboardHistoryClient
    : public crosapi::mojom::ClipboardHistoryClient {
 public:
  // crosapi::mojom::ClipboardHistoryClient:
  MOCK_METHOD(void,
              SetClipboardHistoryItemDescriptors,
              (std::vector<crosapi::mojom::ClipboardHistoryItemDescriptorPtr>),
              (override));

  mojo::Receiver<crosapi::mojom::ClipboardHistoryClient> receiver_{this};
};

}  // namespace

class ClipboardHistoryAshTest : public testing::Test {
 public:
  ClipboardHistoryAsh clipboard_history_ash_;
  ash::MockClipboardHistoryController clipboard_history_controller_;
};

// Verifies that `ClipboardHistoryAsh` uses `ash::ClipboardHistoryController` to
// paste clipboard items by ID.
TEST_F(ClipboardHistoryAshTest, PasteClipboardItemById) {
  struct {
    base::UnguessableToken item_id;
    int event_flags;
    crosapi::mojom::ClipboardHistoryControllerShowSource paste_source;
  } test_params[] = {{base::UnguessableToken::Create(), ui::EF_NONE,
                      crosapi::mojom::ClipboardHistoryControllerShowSource::
                          kRenderViewContextMenu},
                     {base::UnguessableToken::Create(), ui::EF_MOUSE_BUTTON,
                      crosapi::mojom::ClipboardHistoryControllerShowSource::
                          kTextfieldContextMenu}};

  for (const auto& [id, event_flags, paste_source] : test_params) {
    EXPECT_CALL(
        clipboard_history_controller_,
        PasteClipboardItemById(id.ToString(), event_flags, paste_source));
    clipboard_history_ash_.PasteClipboardItemById(id, event_flags,
                                                  paste_source);
  }
}

class ClipboardHistoryAshWithClientTest : public ash::AshTestBase {
 public:
  // ash::AshTestBase:
  void SetUp() override {
    // Enable the clipboard history refresh feature.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kClipboardHistoryRefresh,
                              chromeos::features::kJelly},
        /*disabled_features=*/{});
    ash::AshTestBase::SetUp();

    // `clipboard_history_ash_` should be created after Ash.
    clipboard_history_ash_ = std::make_unique<ClipboardHistoryAsh>();
  }

  // Waits until `mock_client_` receives descriptors. Returns the received
  // descriptors.
  std::vector<mojom::ClipboardHistoryItemDescriptorPtr>
  WaitForReceivedDescriptors() {
    base::test::TestFuture<
        std::vector<mojom::ClipboardHistoryItemDescriptorPtr>>
        future;
    EXPECT_CALL(mock_client_, SetClipboardHistoryItemDescriptors)
        .WillOnce(InvokeFuture(future));

    return future.Take();
  }

  void RegisterClient() {
    clipboard_history_ash_->RegisterClient(
        mock_client_.receiver_.BindNewPipeAndPassRemote());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  MockedClipboardHistoryClient mock_client_;
  std::unique_ptr<ClipboardHistoryAsh> clipboard_history_ash_;
};

// Verifies that the clipboard history client receives clipboard history item
// descriptors from Ash as expected.
TEST_F(ClipboardHistoryAshWithClientTest, Basics) {
  RegisterClient();
  std::vector<std::vector<mojom::ClipboardHistoryItemDescriptor>> test_cases = {
      // Test case 1: The injected descriptors have different formats.
      {
          {base::UnguessableToken::Create(),
           mojom::ClipboardHistoryDisplayFormat::kText, u"dummy text",
           /*file_count=*/0u},
          {base::UnguessableToken::Create(),
           mojom::ClipboardHistoryDisplayFormat::kPng, u"dummy png",
           /*file_count=*/0u},
          {base::UnguessableToken::Create(),
           mojom::ClipboardHistoryDisplayFormat::kHtml, u"dummy html",
           /*file_count=*/0u},
          {base::UnguessableToken::Create(),
           mojom::ClipboardHistoryDisplayFormat::kFile, u"dummy file",
           /*file_count=*/1u},
      },
      // Test case 2: The injected descriptors are text descriptors with special
      // contents.
      {
          // Element 1: A text descriptor with 100 null characters.
          {base::UnguessableToken::Create(),
           mojom::ClipboardHistoryDisplayFormat::kText,
           std::u16string(100, '\0'),
           /*file_count=*/0u},
          // Element 2: A text descriptor with 100 control characters.
          {base::UnguessableToken::Create(),
           mojom::ClipboardHistoryDisplayFormat::kText, std::u16string(100, 1),
           /*file_count=*/0u},
      },
      // Test case 3: The injected descriptors contains the descriptors of
      // unknown types.
      {
          {base::UnguessableToken::Create(),
           mojom::ClipboardHistoryDisplayFormat::kText, u"dummy text",
           /*file_count=*/0u},
          {base::UnguessableToken::Create(),
           mojom::ClipboardHistoryDisplayFormat::kUnknown, u"dummy text2",
           /*file_count=*/0u},
          {base::UnguessableToken::Create(),
           mojom::ClipboardHistoryDisplayFormat::kUnknown, u"dummy text3",
           /*file_count=*/0u},
      },
      // Test case 4: The injected descriptor array is empty.
      {}};

  for (const auto& input_descriptors : test_cases) {
    // Inject the test input through the custom description query callback.
    chromeos::clipboard_history::SetQueryItemDescriptorsImpl(
        chromeos::clipboard_history::QueryItemDescriptorsImpl());
    chromeos::clipboard_history::SetQueryItemDescriptorsImpl(
        base::BindLambdaForTesting(
            [&input_descriptors]() { return input_descriptors; }));

    // Trigger the update to the client.
    clipboard_history_ash_->UpdateRemoteDescriptorsForTesting();

    // Check the clipboard history item descriptors that the client receives.
    auto received_ptrs = WaitForReceivedDescriptors();
    std::vector<mojom::ClipboardHistoryItemDescriptor> received_descriptors;
    for (const auto& ptr : received_ptrs) {
      received_descriptors.emplace_back(ptr->item_id, ptr->display_format,
                                        ptr->display_text, ptr->file_count);
    }
    EXPECT_EQ(input_descriptors, received_descriptors);
  }
}

// Verifies that the client should receive the expected descriptors when
// clipboard history availability is toggled.
TEST_F(ClipboardHistoryAshWithClientTest, ClipboardHistoryAvailabilityToggled) {
  RegisterClient();
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteText(u"A");
  }

  EXPECT_THAT(WaitForReceivedDescriptors(),
              ElementsAre(DescriptorMatches(
                  crosapi::mojom::ClipboardHistoryDisplayFormat::kText, u"A")));

  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteText(u"B");
  }

  EXPECT_THAT(
      WaitForReceivedDescriptors(),
      ElementsAre(
          DescriptorMatches(
              crosapi::mojom::ClipboardHistoryDisplayFormat::kText, u"B"),
          DescriptorMatches(
              crosapi::mojom::ClipboardHistoryDisplayFormat::kText, u"A")));

  // Lock the screen. Clipboard history is no longer available, and its clients
  // should be updated accordingly.
  ash::TestSessionControllerClient* test_session_client =
      GetSessionControllerClient();
  test_session_client->SetSessionState(session_manager::SessionState::LOCKED);

  // `mock_client` should receive an empty array of descriptors.
  EXPECT_TRUE(WaitForReceivedDescriptors().empty());

  // Activate the session. `mock_client` should receive the descriptors that
  // were received before the session lock.
  test_session_client->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_THAT(
      WaitForReceivedDescriptors(),
      ElementsAre(
          DescriptorMatches(
              crosapi::mojom::ClipboardHistoryDisplayFormat::kText, u"B"),
          DescriptorMatches(
              crosapi::mojom::ClipboardHistoryDisplayFormat::kText, u"A")));
}

// Verifies that the client should receive descriptors after registration
// if there are clipboard history items.
TEST_F(ClipboardHistoryAshWithClientTest, RegisterWithNonEmptyHistoryItems) {
  const std::vector<mojom::ClipboardHistoryItemDescriptor> initial_descriptors =
      {
          {base::UnguessableToken::Create(),
           mojom::ClipboardHistoryDisplayFormat::kText, u"dummy text",
           /*file_count=*/0u},
          {base::UnguessableToken::Create(),
           mojom::ClipboardHistoryDisplayFormat::kPng, u"dummy png",
           /*file_count=*/0u},
      };

  chromeos::clipboard_history::SetQueryItemDescriptorsImpl(
      chromeos::clipboard_history::QueryItemDescriptorsImpl());
  chromeos::clipboard_history::SetQueryItemDescriptorsImpl(
      base::BindLambdaForTesting(
          [&initial_descriptors]() { return initial_descriptors; }));

  RegisterClient();
  EXPECT_THAT(
      WaitForReceivedDescriptors(),
      ElementsAre(
          DescriptorMatches(
              crosapi::mojom::ClipboardHistoryDisplayFormat::kText,
              u"dummy text"),
          DescriptorMatches(crosapi::mojom::ClipboardHistoryDisplayFormat::kPng,
                            u"dummy png")));
}

// Verifies that the client should not receive any remote calls from Ash on
// descriptor update after registration if clipboard history is empty.
TEST_F(ClipboardHistoryAshWithClientTest, RegisterWithEmptyHistoryItems) {
  EXPECT_CALL(mock_client_, SetClipboardHistoryItemDescriptors).Times(0);
  RegisterClient();
  clipboard_history_ash_->FlushForTesting();
}

}  // namespace crosapi
