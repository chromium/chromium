// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/annotator_page_handler_impl.h"

#include "ash/public/cpp/projector/annotator_tool.h"
#include "ash/public/cpp/test/mock_projector_controller.h"
#include "ash/webui/projector_app/mojom/annotator.mojom.h"
#include "ash/webui/projector_app/public/mojom/annotator_structs.mojom.h"
#include "ash/webui/projector_app/test/mock_app_client.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

namespace {

// MOCK the annotator instance in the WebUI renderer.
class MockAnnotatorPage : public annotator::mojom::AnnotatorPage {
 public:
  MockAnnotatorPage() = default;
  MockAnnotatorPage(const MockAnnotatorPage&) = delete;
  MockAnnotatorPage& operator=(const MockAnnotatorPage&) = delete;
  ~MockAnnotatorPage() override = default;

  MOCK_METHOD0(Clear, void());
  MOCK_METHOD0(Undo, void());
  MOCK_METHOD0(Redo, void());
  MOCK_METHOD1(SetTool, void(annotator::mojom::AnnotatorToolPtr tool));

  void FlushReceiverForTesting() { receiver_.FlushForTesting(); }

  void FlushRemoteForTesting() { remote_.FlushForTesting(); }

  void SendUndoRedoAvailableChanged(bool undo_available, bool redo_available) {
    remote_->OnUndoRedoAvailabilityChanged(undo_available, redo_available);
  }

  void SendCanvasInitialized(bool success) {
    remote_->OnCanvasInitialized(success);
  }

  mojo::Receiver<annotator::mojom::AnnotatorPage>& receiver() {
    return receiver_;
  }
  mojo::Remote<annotator::mojom::AnnotatorPageHandler>& remote() {
    return remote_;
  }

 private:
  mojo::Receiver<annotator::mojom::AnnotatorPage> receiver_{this};
  mojo::Remote<annotator::mojom::AnnotatorPageHandler> remote_;
};

}  // namespace

class AnnotatorPageHandlerImplTest : public testing::Test {
 public:
  AnnotatorPageHandlerImplTest() = default;
  AnnotatorPageHandlerImplTest(const AnnotatorPageHandlerImplTest&) = delete;
  AnnotatorPageHandlerImplTest& operator=(const AnnotatorPageHandlerImplTest&) =
      delete;
  ~AnnotatorPageHandlerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    annotator_ = std::make_unique<MockAnnotatorPage>();
    handler_ = std::make_unique<AnnotatorPageHandlerImpl>(
        annotator().remote().BindNewPipeAndPassReceiver(),
        annotator().receiver().BindNewPipeAndPassRemote(),
        /*web_ui=*/nullptr);
  }

  void TearDown() override {
    annotator_.reset();
    handler_.reset();
  }

  AnnotatorPageHandlerImpl& handler() { return *handler_; }
  MockProjectorController& controller() { return controller_; }
  MockAnnotatorPage& annotator() { return *annotator_; }
  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<MockAnnotatorPage> annotator_;
  std::unique_ptr<AnnotatorPageHandlerImpl> handler_;
  MockProjectorController controller_;
  MockAppClient client_;
};

TEST_F(AnnotatorPageHandlerImplTest, SetTool) {
  AnnotatorTool expected_tool;
  expected_tool.color = SkColorSetARGB(0xA1, 0xB2, 0xC3, 0xD4);
  expected_tool.size = 5;
  expected_tool.type = AnnotatorToolType::kPen;
  EXPECT_CALL(annotator(), SetTool)
      .WillOnce(testing::Invoke([&](annotator::mojom::AnnotatorToolPtr tool) {
        EXPECT_EQ(tool->size, expected_tool.size);
        EXPECT_EQ(tool->tool, expected_tool.GetToolString());
        EXPECT_EQ(tool->color, expected_tool.GetColorHexString());
      }));

  handler().SetTool(expected_tool);
  annotator().FlushReceiverForTesting();
}

TEST_F(AnnotatorPageHandlerImplTest, Undo) {
  EXPECT_CALL(annotator(), Undo());
  handler().Undo();
  annotator().FlushReceiverForTesting();
}

TEST_F(AnnotatorPageHandlerImplTest, Redo) {
  EXPECT_CALL(annotator(), Redo());
  handler().Redo();
  annotator().FlushReceiverForTesting();
}

TEST_F(AnnotatorPageHandlerImplTest, Clear) {
  EXPECT_CALL(annotator(), Clear());
  handler().Clear();
  annotator().FlushReceiverForTesting();
}

TEST_F(AnnotatorPageHandlerImplTest, UndoRedoAvailabilityChanged) {
  EXPECT_CALL(controller(), OnUndoRedoAvailabilityChanged(false, false));
  annotator().SendUndoRedoAvailableChanged(false, false);

  EXPECT_CALL(controller(), OnUndoRedoAvailabilityChanged(true, true));
  annotator().SendUndoRedoAvailableChanged(true, true);

  EXPECT_CALL(controller(), OnUndoRedoAvailabilityChanged(false, true));
  annotator().SendUndoRedoAvailableChanged(false, true);
  annotator().FlushRemoteForTesting();
}

TEST_F(AnnotatorPageHandlerImplTest, CanvasInitialized) {
  EXPECT_CALL(controller(), OnCanvasInitialized(true));
  annotator().SendCanvasInitialized(true);

  EXPECT_CALL(controller(), OnCanvasInitialized(false));
  annotator().SendCanvasInitialized(false);
  annotator().FlushRemoteForTesting();
}

}  // namespace ash
