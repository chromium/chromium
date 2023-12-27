// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_keyboard_layout_handler.h"

#include "ash/ime/ime_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash::eche_app {

namespace {

class TaskRunner {
 public:
  TaskRunner() = default;
  ~TaskRunner() = default;

  void WaitForResult() { run_loop_.Run(); }

  void Finish() { run_loop_.Quit(); }

 private:
  base::RunLoop run_loop_;
};

class FakeKeyboardLayoutObserver : public mojom::KeyboardLayoutObserver {
 public:
  FakeKeyboardLayoutObserver(
      mojo::PendingRemote<mojom::KeyboardLayoutObserver>* remote,
      TaskRunner* task_runner)
      : receiver_(this, remote->InitWithNewPipeAndPassReceiver()) {
    task_runner_ = task_runner;
  }
  ~FakeKeyboardLayoutObserver() override = default;

  // mojom::KeyboardLayoutObserver:
  void OnKeyboardLayoutChanged(const std::string& id,
                               const std::string& long_name,
                               const std::string& short_name,
                               const std::string& layout_tag) override {
    ++num_keyboard_layout_information_sent_;
    task_runner_->Finish();
  }

  size_t num_keyboard_layout_information_sent() const {
    return num_keyboard_layout_information_sent_;
  }

 private:
  size_t num_keyboard_layout_information_sent_ = 0;
  raw_ptr<TaskRunner> task_runner_;
  mojo::Receiver<mojom::KeyboardLayoutObserver> receiver_;
};

}  // namespace

class EcheKeyboardLayoutHandlerTest : public AshTestBase {
 public:
  EcheKeyboardLayoutHandlerTest() = default;
  EcheKeyboardLayoutHandlerTest(const EcheKeyboardLayoutHandlerTest&) = delete;
  EcheKeyboardLayoutHandlerTest& operator=(
      const EcheKeyboardLayoutHandlerTest&) = delete;
  ~EcheKeyboardLayoutHandlerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    DCHECK(test_web_view_factory_.get());
    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();

    keyboard_handler_ = std::make_unique<EcheKeyboardLayoutHandler>();
  }

  void TearDown() override {
    keyboard_handler_.reset();
    AshTestBase::TearDown();
  }

  void RequestKeyboardLayout() {
    keyboard_handler_->RequestCurrentKeyboardLayout();
  }

  void SetKeyboardLayoutObserver(
      mojo::PendingRemote<mojom::KeyboardLayoutObserver> observer) {
    keyboard_handler_->SetKeyboardLayoutObserver(std::move(observer));
  }

  void ChangeKeyboardLayout() {
    ImeControllerImpl* ime_controller = Shell::Get()->ime_controller();
    ime_controller->OnKeyboardLayoutNameChanged(
        ime_controller->keyboard_layout_name());
  }

  TaskRunner task_runner_;

 private:
  std::unique_ptr<EcheKeyboardLayoutHandler> keyboard_handler_ = nullptr;
  std::unique_ptr<TestAshWebViewFactory> test_web_view_factory_ =
      std::make_unique<TestAshWebViewFactory>();
};

TEST_F(EcheKeyboardLayoutHandlerTest, RequestCurrentKeyboardLayout) {
  mojo::PendingRemote<mojom::KeyboardLayoutObserver> observer;
  FakeKeyboardLayoutObserver fake_keyboard_layout_observer(&observer,
                                                           &task_runner_);
  SetKeyboardLayoutObserver(std::move(observer));

  RequestKeyboardLayout();
  task_runner_.WaitForResult();

  EXPECT_EQ(
      1u, fake_keyboard_layout_observer.num_keyboard_layout_information_sent());
}

TEST_F(EcheKeyboardLayoutHandlerTest, OnKeyboardLayoutNameChanged) {
  mojo::PendingRemote<mojom::KeyboardLayoutObserver> observer;
  FakeKeyboardLayoutObserver fake_keyboard_layout_observer(&observer,
                                                           &task_runner_);
  SetKeyboardLayoutObserver(std::move(observer));

  ChangeKeyboardLayout();
  task_runner_.WaitForResult();

  EXPECT_EQ(
      1u, fake_keyboard_layout_observer.num_keyboard_layout_information_sent());
}

TEST_F(EcheKeyboardLayoutHandlerTest, NoResultsIfRemoteNotSet) {
  mojo::PendingRemote<mojom::KeyboardLayoutObserver> observer;
  FakeKeyboardLayoutObserver fake_keyboard_layout_observer(&observer,
                                                           &task_runner_);

  RequestKeyboardLayout();
  EXPECT_EQ(
      0u, fake_keyboard_layout_observer.num_keyboard_layout_information_sent());

  ChangeKeyboardLayout();
  EXPECT_EQ(
      0u, fake_keyboard_layout_observer.num_keyboard_layout_information_sent());
}

}  // namespace ash::eche_app
