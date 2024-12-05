// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/global_shortcut_listener.h"

#include <memory>

#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace extensions {

namespace {

// Test implementation of GlobalShortcutListener that doesn't delegate
// to an OS-specific implementation. All this does is fail to register an
// accelerator if it has already been registered.
class GlobalShortcutListenerForTesting final : public GlobalShortcutListener {
 public:
  void StartListening() override {}
  void StopListening() override {}

  bool RegisterAcceleratorImpl(const ui::Accelerator& accelerator) override {
    if (registered_accelerators_.contains(accelerator)) {
      return false;
    }

    registered_accelerators_.insert(accelerator);
    return true;
  }

  void UnregisterAcceleratorImpl(const ui::Accelerator& accelerator) override {
    registered_accelerators_.erase(accelerator);
  }

 private:
  std::set<ui::Accelerator> registered_accelerators_;
};

class TestObserver final : public GlobalShortcutListener::Observer {
 public:
  virtual ~TestObserver() = default;

  void OnKeyPressed(const ui::Accelerator& accelerator) override {}

  void ExecuteCommand(const ExtensionId& extension_id,
                      const std::string& command_id) override {}
};

class GlobalShortcutListenerTest : public testing::Test {
 public:
  GlobalShortcutListenerTest()
      : task_environment_(content::BrowserTaskEnvironment::MainThreadType::UI) {
    listener_ = std::make_unique<GlobalShortcutListenerForTesting>();
  }

  GlobalShortcutListenerTest(const GlobalShortcutListenerTest&) = delete;
  GlobalShortcutListenerTest& operator=(const GlobalShortcutListenerTest&) =
      delete;

  void SetUp() override {
    listener_->SetShortcutHandlingSuspended(false);
    observer_ = std::make_unique<TestObserver>();
  }
  void TearDown() override {
    observer_ = nullptr;
    listener_ = nullptr;
  }

  GlobalShortcutListener::Observer* GetObserver() { return observer_.get(); }

  GlobalShortcutListener* GetListener() { return listener_.get(); }

 private:
  std::unique_ptr<GlobalShortcutListenerForTesting> listener_;
  std::unique_ptr<TestObserver> observer_ = nullptr;
  // A UI environment is required since GlobalShortcutListener (base class of
  // GlobalShortcutListenerLinux) CHECKs that it's running on a UI thread.
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(GlobalShortcutListenerTest, RegistersAccelerators) {
  GlobalShortcutListener* listener = GetListener();
  const ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);

  // First registration attempt succeeds.
  EXPECT_TRUE(listener->RegisterAccelerator(accelerator_a, GetObserver()));

  // A second registration fails because the accelerator is already registered.
  EXPECT_FALSE(listener->RegisterAccelerator(accelerator_a, GetObserver()));

  // Clean up registration.
  listener->UnregisterAccelerator(accelerator_a, GetObserver());
}

TEST_F(GlobalShortcutListenerTest, SuspendsShortcutHandling) {
  GlobalShortcutListener* listener = GetListener();
  const ui::Accelerator accelerator_b(ui::VKEY_B, ui::EF_NONE);

  listener->SetShortcutHandlingSuspended(true);
  EXPECT_TRUE(listener->IsShortcutHandlingSuspended());

  // Can't register accelerator while shortcut handling is suspended.
  EXPECT_FALSE(listener->RegisterAccelerator(accelerator_b, GetObserver()));

  listener->SetShortcutHandlingSuspended(false);
  EXPECT_FALSE(listener->IsShortcutHandlingSuspended());

  // Can register accelerator when shortcut handling isn't suspended.
  EXPECT_TRUE(listener->RegisterAccelerator(accelerator_b, GetObserver()));

  // Clean up registration.
  listener->UnregisterAccelerator(accelerator_b, GetObserver());
}

}  // namespace
}  // namespace extensions
