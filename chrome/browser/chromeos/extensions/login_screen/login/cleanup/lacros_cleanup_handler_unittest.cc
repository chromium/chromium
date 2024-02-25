// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/lacros_cleanup_handler.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chromeos/crosapi/mojom/login.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::WithArg;

namespace chromeos {

namespace {

class TestCleanupTriggeredObserver
    : public crosapi::mojom::LacrosCleanupTriggeredObserver {
 public:
  explicit TestCleanupTriggeredObserver(std::optional<std::string> error)
      : error_(error) {}

  explicit TestCleanupTriggeredObserver(bool should_reset)
      : should_reset_(should_reset) {}

  void OnLacrosCleanupTriggered(
      OnLacrosCleanupTriggeredCallback callback) override {
    if (should_reset_) {
      receiver_->reset();
      // Note that `callback` is not run since the Mojo pipe disconnects.
      return;
    }

    std::move(callback).Run(error_);
  }

  void SetReceiver(
      mojo::Receiver<crosapi::mojom::LacrosCleanupTriggeredObserver>*
          receiver) {
    receiver_ = receiver;
  }

  std::optional<std::string> error_;
  bool should_reset_ = false;
  raw_ptr<mojo::Receiver<crosapi::mojom::LacrosCleanupTriggeredObserver>,
          DanglingUntriaged>
      receiver_;
};

}  // namespace

class LacrosCleanupHandlerUnittest : public testing::Test {
 public:
  LacrosCleanupHandlerUnittest() = default;
  ~LacrosCleanupHandlerUnittest() override = default;

  void SetUp() override {
    testing::Test::SetUp();

    lacros_cleanup_handler_ = std::make_unique<LacrosCleanupHandler>();
    lacros_cleanup_handler_->SetCleanupTriggeredObserversForTesting(
        &remote_set_);
  }

  void SetUpLacrosCleanupTriggeredObserver(
      std::unique_ptr<TestCleanupTriggeredObserver> observer) {
    // Set up pending receiver and remote.
    mojo::PendingReceiver<crosapi::mojom::LacrosCleanupTriggeredObserver>
        pending_receiver;
    mojo::PendingRemote<crosapi::mojom::LacrosCleanupTriggeredObserver>
        pending_remote = pending_receiver.InitWithNewPipeAndPassRemote();

    // Create receiver bound to test observer.
    observer_ = std::move(observer);
    receiver_ = std::make_unique<
        mojo::Receiver<crosapi::mojom::LacrosCleanupTriggeredObserver>>(
        observer_.get(), std::move(pending_receiver));
    observer_->SetReceiver(receiver_.get());

    remote_set_.Add(std::move(pending_remote));
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<LacrosCleanupHandler> lacros_cleanup_handler_;
  mojo::RemoteSet<crosapi::mojom::LacrosCleanupTriggeredObserver> remote_set_;
  std::unique_ptr<TestCleanupTriggeredObserver> observer_;
  std::unique_ptr<
      mojo::Receiver<crosapi::mojom::LacrosCleanupTriggeredObserver>>
      receiver_;
};

TEST_F(LacrosCleanupHandlerUnittest, Cleanup) {
  SetUpLacrosCleanupTriggeredObserver(
      std::make_unique<TestCleanupTriggeredObserver>(std::nullopt));

  base::test::TestFuture<std::optional<std::string>> future;
  lacros_cleanup_handler_->Cleanup(
      future.GetCallback<const std::optional<std::string>&>());

  EXPECT_EQ(std::nullopt, future.Get());
}

TEST_F(LacrosCleanupHandlerUnittest, CleanupError) {
  std::string error("error");
  SetUpLacrosCleanupTriggeredObserver(
      std::make_unique<TestCleanupTriggeredObserver>(error));

  base::test::TestFuture<std::optional<std::string>> future;
  lacros_cleanup_handler_->Cleanup(
      future.GetCallback<const std::optional<std::string>&>());

  EXPECT_EQ(error, future.Get());
}

TEST_F(LacrosCleanupHandlerUnittest, NoObservers) {
  base::test::TestFuture<std::optional<std::string>> future;
  lacros_cleanup_handler_->Cleanup(
      future.GetCallback<const std::optional<std::string>&>());

  EXPECT_EQ(std::nullopt, future.Get());
}

TEST_F(LacrosCleanupHandlerUnittest, Disconnect) {
  SetUpLacrosCleanupTriggeredObserver(
      std::make_unique<TestCleanupTriggeredObserver>(/*should_reset=*/true));

  base::test::TestFuture<std::optional<std::string>> future;
  lacros_cleanup_handler_->Cleanup(
      future.GetCallback<const std::optional<std::string>&>());

  EXPECT_EQ(std::nullopt, future.Get());
}

}  // namespace chromeos
