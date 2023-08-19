// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/clipboard_history_lacros.h"

#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/startup/browser_init_params.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"

namespace crosapi {

namespace {

using ::testing::ElementsAre;
using ::testing::Mock;

// Matchers --------------------------------------------------------------------

MATCHER_P2(MatchDescriptor, display_format, display_text, "") {
  return arg.display_format == display_format &&
         arg.display_text == display_text;
}

// Helper classes --------------------------------------------------------------

class MockClipboardHistoryAsh : public mojom::ClipboardHistory {
 public:
  MockClipboardHistoryAsh() {
    ON_CALL(*this, RegisterClient)
        .WillByDefault(testing::Invoke(
            [&](mojo::PendingRemote<mojom::ClipboardHistoryClient> client) {
              remote_.Bind(std::move(client));
            }));
  }

  // mojom::ClipboardHistory:
  MOCK_METHOD(void,
              ShowClipboard,
              (const gfx::Rect&,
               ui::MenuSourceType,
               mojom::ClipboardHistoryControllerShowSource),
              (override));
  MOCK_METHOD(void,
              PasteClipboardItemById,
              (const base::UnguessableToken&,
               int,
               mojom::ClipboardHistoryControllerShowSource),
              (override));
  MOCK_METHOD(void,
              RegisterClient,
              (mojo::PendingRemote<mojom::ClipboardHistoryClient>),
              (override));

  mojo::Receiver<mojom::ClipboardHistory> receiver_{this};

  mojo::Remote<mojom::ClipboardHistoryClient> remote_;
};

}  // namespace

class ClipboardHistoryLacrosTest : public testing::Test {
 public:
  // testing::Test:
  void SetUp() override {
    // Enable the clipboard history refresh feature.
    crosapi::mojom::BrowserInitParamsPtr params_ptr =
        crosapi::mojom::BrowserInitParams::New();
    params_ptr->enable_clipboard_history_refresh = true;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(params_ptr));
  }

  MockClipboardHistoryAsh mock_clipboard_history_ash_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ClipboardHistoryLacrosTest, Basics) {
  // Verifies that `ClipboardHistoryLacros` calls the clipboard history
  // interface to register itself as a client.
  EXPECT_CALL(mock_clipboard_history_ash_, RegisterClient).Times(1);
  ClipboardHistoryLacros client(&mock_clipboard_history_ash_);
  Mock::VerifyAndClearExpectations(&mock_clipboard_history_ash_);

  std::vector<crosapi::mojom::ClipboardHistoryItemDescriptorPtr>
      descriptor_ptrs_from_ash;
  descriptor_ptrs_from_ash.push_back(
      crosapi::mojom::ClipboardHistoryItemDescriptor::New(
          base::UnguessableToken::Create(),
          mojom::ClipboardHistoryDisplayFormat::kText, u"A",
          /*file_count=*/0u));
  descriptor_ptrs_from_ash.push_back(
      crosapi::mojom::ClipboardHistoryItemDescriptor::New(
          base::UnguessableToken::Create(),
          mojom::ClipboardHistoryDisplayFormat::kHtml, u"HTML",
          /*file_count=*/0u));
  descriptor_ptrs_from_ash.push_back(
      crosapi::mojom::ClipboardHistoryItemDescriptor::New(
          base::UnguessableToken::Create(),
          mojom::ClipboardHistoryDisplayFormat::kUnknown, u"garbage",
          /*file_count=*/0u));

  // Send a non-empty descriptor array from Ash. Verify the descriptors cached
  // on Lacros. The descriptors of unknown types should be filtered out.
  mock_clipboard_history_ash_.remote_->SetClipboardHistoryItemDescriptors(
      std::move(descriptor_ptrs_from_ash));
  mock_clipboard_history_ash_.remote_.FlushForTesting();
  EXPECT_THAT(
      client.cached_descriptors(),
      ElementsAre(
          MatchDescriptor(crosapi::mojom::ClipboardHistoryDisplayFormat::kText,
                          u"A"),
          MatchDescriptor(crosapi::mojom::ClipboardHistoryDisplayFormat::kHtml,
                          u"HTML")));

  // Send an empty descriptor array from Ash. Verify the descriptors cached
  // on Lacros.
  mock_clipboard_history_ash_.remote_->SetClipboardHistoryItemDescriptors({});
  mock_clipboard_history_ash_.remote_.FlushForTesting();
  EXPECT_TRUE(client.cached_descriptors().empty());
}

}  // namespace crosapi
