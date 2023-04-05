// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/untrusted_projector_page_handler_impl.h"

#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "ash/public/cpp/test/mock_projector_controller.h"
#include "ash/webui/projector_app/mojom/untrusted_projector.mojom.h"
#include "ash/webui/projector_app/public/mojom/projector_types.mojom.h"
#include "ash/webui/projector_app/test/mock_app_client.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// MOCK the Projector page instance in the WebUI renderer.
class MockUntrustedProjectorPageJs
    : public projector::mojom::UntrustedProjectorPage {
 public:
  MockUntrustedProjectorPageJs() = default;
  MockUntrustedProjectorPageJs(const MockUntrustedProjectorPageJs&) = delete;
  MockUntrustedProjectorPageJs& operator=(const MockUntrustedProjectorPageJs&) =
      delete;
  ~MockUntrustedProjectorPageJs() override = default;

  MOCK_METHOD1(
      OnNewScreencastPreconditionChanged,
      void(ash::projector::mojom::NewScreencastPreconditionPtr precondition));

  void FlushReceiverForTesting() { receiver_.FlushForTesting(); }

  void FlushRemoteForTesting() { page_handler_.FlushForTesting(); }

  mojo::Receiver<projector::mojom::UntrustedProjectorPage>& receiver() {
    return receiver_;
  }
  mojo::Remote<projector::mojom::UntrustedProjectorPageHandler>&
  page_handler() {
    return page_handler_;
  }

 private:
  mojo::Receiver<projector::mojom::UntrustedProjectorPage> receiver_{this};
  mojo::Remote<projector::mojom::UntrustedProjectorPageHandler> page_handler_;
};

}  // namespace

class UntrustedProjectorPageHandlerImplUnitTest : public testing::Test {
 public:
  UntrustedProjectorPageHandlerImplUnitTest() = default;
  UntrustedProjectorPageHandlerImplUnitTest(
      const UntrustedProjectorPageHandlerImplUnitTest&) = delete;
  UntrustedProjectorPageHandlerImplUnitTest& operator=(
      const UntrustedProjectorPageHandlerImplUnitTest&) = delete;
  ~UntrustedProjectorPageHandlerImplUnitTest() override = default;

  void SetUp() override {
    page_ = std::make_unique<MockUntrustedProjectorPageJs>();
    handler_impl_ = std::make_unique<UntrustedProjectorPageHandlerImpl>(
        page().page_handler().BindNewPipeAndPassReceiver(),
        page().receiver().BindNewPipeAndPassRemote());
  }

  void TearDown() override {
    handler_impl_.reset();
    page_.reset();
  }

  MockProjectorController& controller() { return mock_controller_; }
  MockUntrustedProjectorPageJs& page() { return *page_; }
  UntrustedProjectorPageHandlerImpl& handler() { return *handler_impl_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  MockProjectorController mock_controller_;
  MockAppClient mock_app_client_;
  std::unique_ptr<MockUntrustedProjectorPageJs> page_;
  std::unique_ptr<UntrustedProjectorPageHandlerImpl> handler_impl_;
};

TEST_F(UntrustedProjectorPageHandlerImplUnitTest, CanStartProjectorSession) {
  NewScreencastPrecondition precondition = NewScreencastPrecondition(
      NewScreencastPreconditionState::kEnabled,
      {NewScreencastPreconditionReason::kEnabledBySoda});

  ON_CALL(controller(), GetNewScreencastPrecondition)
      .WillByDefault(testing::Return(precondition));

  base::RunLoop runloop;
  page().page_handler()->GetNewScreencastPrecondition(
      base::BindLambdaForTesting(
          [&](projector::mojom::NewScreencastPreconditionPtr precondition) {
            EXPECT_EQ(
                precondition->state,
                projector::mojom::NewScreencastPreconditionState::kEnabled);
            EXPECT_EQ(precondition->reasons.size(), 1u);
            EXPECT_EQ(precondition->reasons[0],
                      projector::mojom::NewScreencastPreconditionReason::
                          kEnabledBySoda);
            runloop.Quit();
          }));

  runloop.Run();
}

TEST_F(UntrustedProjectorPageHandlerImplUnitTest,
       NewScreencastPreconditionChanged) {
  EXPECT_CALL(page(), OnNewScreencastPreconditionChanged(testing::_)).Times(1);
  NewScreencastPrecondition precondition = NewScreencastPrecondition(
      NewScreencastPreconditionState::kEnabled,
      {NewScreencastPreconditionReason::kEnabledBySoda});
  handler().OnNewScreencastPreconditionChanged(precondition);
  page().FlushReceiverForTesting();
}

}  // namespace ash
