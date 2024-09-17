// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/annotator/annotator_client_impl.h"

#include "ash/public/cpp/annotator/annotator_tool.h"
#include "ash/public/cpp/test/mock_annotator_controller.h"
#include "ash/webui/annotator/mojom/untrusted_annotator.mojom.h"
#include "ash/webui/annotator/public/mojom/annotator_structs.mojom.h"
#include "ash/webui/annotator/test/mock_untrusted_annotator_page.h"
#include "ash/webui/annotator/untrusted_annotator_page_handler_impl.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class AnnotatorClientImplTest : public testing::Test {
 public:
  AnnotatorClientImplTest() = default;
  AnnotatorClientImplTest(const AnnotatorClientImplTest&) = delete;
  AnnotatorClientImplTest& operator=(const AnnotatorClientImplTest&) = delete;
  ~AnnotatorClientImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    annotator_client_ =
        std::make_unique<AnnotatorClientImpl>(&annotator_controller_);
    annotator_ = std::make_unique<MockUntrustedAnnotatorPage>();
    handler_ = std::make_unique<UntrustedAnnotatorPageHandlerImpl>(
        annotator().remote().BindNewPipeAndPassReceiver(),
        annotator().receiver().BindNewPipeAndPassRemote(),
        /*web_ui=*/nullptr);

    // Annotator client has the handler's reference at this point, as it is set
    // in the handler's constructor.
    EXPECT_EQ(handler_.get(),
              annotator_client_->get_annotator_handler_for_test());
  }

  void TearDown() override {
    handler_.reset();
    annotator_.reset();
    annotator_client_.reset();
  }

  AnnotatorClientImpl& annotator_client() { return *annotator_client_; }
  MockUntrustedAnnotatorPage& annotator() { return *annotator_; }
  UntrustedAnnotatorPageHandlerImpl* handler() { return handler_.get(); }
  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<MockUntrustedAnnotatorPage> annotator_;
  std::unique_ptr<UntrustedAnnotatorPageHandlerImpl> handler_;
  std::unique_ptr<AnnotatorClientImpl> annotator_client_;
  MockAnnotatorController annotator_controller_;
};

TEST_F(AnnotatorClientImplTest, SetTool) {
  AnnotatorTool expected_tool;
  expected_tool.size = 5;
  EXPECT_CALL(annotator(), SetTool)
      .WillOnce(testing::Invoke([&](annotator::mojom::AnnotatorToolPtr tool) {
        EXPECT_EQ(tool->size, expected_tool.size);
      }));

  annotator_client().SetTool(expected_tool);
  annotator().FlushReceiverForTesting();
}

TEST_F(AnnotatorClientImplTest, Clear) {
  EXPECT_CALL(annotator(), Clear());
  annotator_client().Clear();
  annotator().FlushReceiverForTesting();
}

TEST_F(AnnotatorClientImplTest, ResetAnnotatorPageHandler) {
  annotator_client().ResetAnnotatorPageHandler(handler());
  EXPECT_EQ(annotator_client().get_annotator_handler_for_test(), nullptr);
}

}  // namespace ash
