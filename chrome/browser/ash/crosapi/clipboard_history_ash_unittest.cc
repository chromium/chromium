// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/clipboard_history_ash.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/clipboard/test_support/mock_clipboard_history_controller.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom-shared.h"
#include "chromeos/ui/clipboard_history/clipboard_history_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"

namespace crosapi {

namespace {

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

class ClipboardHistoryAshWithClientTest : public ClipboardHistoryAshTest {
 public:
  // ClipboardHistoryAshTest:
  void SetUp() override {
    // Enable the clipboard history refresh feature.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kClipboardHistoryRefresh,
                              chromeos::features::kJelly},
        /*disabled_features=*/{});
    ClipboardHistoryAshTest::SetUp();

    // Initialize `mock_client_`
    mock_client_ = std::make_unique<MockedClipboardHistoryClient>();
    clipboard_history_ash_.RegisterClient(
        mock_client_->receiver_.BindNewPipeAndPassRemote());
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<MockedClipboardHistoryClient> mock_client_;
};

// Verifies that the clipboard history client receives clipboard history item
// descriptors from Ash as expected.
TEST_F(ClipboardHistoryAshWithClientTest, Basics) {
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
    // Inject the test input.
    chromeos::clipboard_history::SetQueryItemDescriptorsImpl(
        base::BindLambdaForTesting(
            [&input_descriptors]() { return input_descriptors; }));

    // Check the clipboard history item descriptors that the client receives.
    EXPECT_CALL(*mock_client_, SetClipboardHistoryItemDescriptors)
        .WillOnce([&input_descriptors](
                      std::vector<mojom::ClipboardHistoryItemDescriptorPtr>
                          received_ptrs) {
          std::vector<mojom::ClipboardHistoryItemDescriptor>
              received_descriptors;
          for (const auto& ptr : received_ptrs) {
            received_descriptors.emplace_back(ptr->item_id, ptr->display_format,
                                              ptr->display_text,
                                              ptr->file_count);
          }
          EXPECT_EQ(input_descriptors, received_descriptors);
        });

    // Update the descriptors on `mock_client_`. It is not a real code path.
    // The real code path is covered by other tests.
    clipboard_history_ash_.UpdateRemoteDescriptorsForTesting();
    clipboard_history_ash_.FlushForTesting();

    chromeos::clipboard_history::SetQueryItemDescriptorsImpl(
        base::NullCallback());
  }
}

}  // namespace crosapi
