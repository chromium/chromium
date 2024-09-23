// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/input_methods_ash.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/crosapi/mojom/input_methods.mojom-forward.h"
#include "chromeos/crosapi/mojom/input_methods.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/mock_input_method_manager.h"

namespace crosapi {

namespace {

class InputMethodManagerFake
    : public ash::input_method::MockInputMethodManager {
 public:
  InputMethodManagerFake() : state_(base::MakeRefCounted<TestState>()) {}

  InputMethodManagerFake(const InputMethodManagerFake&) = delete;
  InputMethodManagerFake& operator=(const InputMethodManagerFake&) = delete;

  void SetEnabledInputMethods(std::vector<std::string> input_methods) {
    state_->SetEnabledInputMethods(std::move(input_methods));
  }

  // Sets callback that will be used in `GetMigratedInputMethodID`.
  void SetInputMethodIdMigrationMethod(
      base::RepeatingCallback<std::string(const std::string&)>
          migration_method) {
    migration_method_ = migration_method;
  }

  std::optional<std::string> GetLastSetInputMethod() const {
    return state_->GetLastSetInputMethod();
  }

 private:
  class TestState : public State {
   public:
    TestState() = default;

    void SetEnabledInputMethods(std::vector<std::string> input_methods) {
      enabled_input_methods_ = std::move(input_methods);
    }

    std::optional<std::string> GetLastSetInputMethod() const {
      return current_input_method_;
    }

    // `State` implementation:
    void ChangeInputMethod(const std::string& value,
                           bool show_message) override {
      current_input_method_ = value;
    }

    const std::vector<std::string>& GetEnabledInputMethodIds() const override {
      return enabled_input_methods_;
    }

   protected:
    friend base::RefCounted<ash::input_method::InputMethodManager::State>;
    ~TestState() override = default;

   private:
    std::vector<std::string> enabled_input_methods_;
    std::optional<std::string> current_input_method_;
  };

  // `MockInputMethodManager` implementation:
  scoped_refptr<ash::input_method::InputMethodManager::State>
  GetActiveIMEState() override {
    return state_;
  }

  std::string GetMigratedInputMethodID(
      const std::string& input_method_id) override {
    return migration_method_.Run(input_method_id);
  }

 private:
  scoped_refptr<TestState> state_;

  base::RepeatingCallback<std::string(const std::string&)> migration_method_ =
      base::BindRepeating([](const std::string& id) { return id; });
};

}  // namespace

class InputMethodsAshTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Ownership of this pointer is taken by `InputMethodManager::Initialize()`.
    input_method_manager_ = new InputMethodManagerFake;
    ash::input_method::InputMethodManager::Initialize(
        input_method_manager_.get());

    input_methods_ash_.BindReceiver(remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    input_method_manager_ = nullptr;
    ash::input_method::InputMethodManager::Shutdown();
  }

  InputMethodManagerFake& input_method_manager() {
    return *input_method_manager_;
  }

  mojo::Remote<mojom::InputMethods>& mojom_remote() { return remote_; }

  bool ChangeInputMethodAndWaitForResponse(
      mojo::Remote<mojom::InputMethods>& remote,
      const std::string& input_method) {
    base::test::TestFuture<bool> future;
    remote->ChangeInputMethod(input_method, future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return future.Get();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  raw_ptr<InputMethodManagerFake> input_method_manager_;
  InputMethodsAsh input_methods_ash_;
  mojo::Remote<mojom::InputMethods> remote_;
};

TEST_F(InputMethodsAshTest, ShouldAcceptEnabledInputMethod) {
  input_method_manager().SetEnabledInputMethods({"en-us", "fr-fr"});

  bool succeeded = ChangeInputMethodAndWaitForResponse(mojom_remote(), "fr-fr");

  EXPECT_TRUE(succeeded);
  EXPECT_EQ(input_method_manager().GetLastSetInputMethod(), "fr-fr");
}

TEST_F(InputMethodsAshTest, ShouldRejectDisableInputMethod) {
  input_method_manager().SetEnabledInputMethods({"en-us", "fr-fr"});

  bool succeeded = ChangeInputMethodAndWaitForResponse(mojom_remote(), "it-it");

  EXPECT_FALSE(succeeded);
  EXPECT_EQ(input_method_manager().GetLastSetInputMethod(), std::nullopt);
}

TEST_F(InputMethodsAshTest, ShouldUseMigratedInputMethodIds) {
  input_method_manager().SetEnabledInputMethods({"migrated-be-nl"});
  input_method_manager().SetInputMethodIdMigrationMethod(base::BindRepeating(
      [](const std::string& id) { return "migrated-" + id; }));

  bool succeeded = ChangeInputMethodAndWaitForResponse(mojom_remote(), "be-nl");

  EXPECT_TRUE(succeeded);
  EXPECT_EQ(input_method_manager().GetLastSetInputMethod(), "migrated-be-nl");
}

}  // namespace crosapi
