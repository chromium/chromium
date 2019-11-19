// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_configuration.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/input_method/accessibility.h"
#include "chrome/browser/chromeos/input_method/browser_state_monitor.h"
#include "chrome/browser/chromeos/input_method/input_method_delegate_impl.h"
#include "chrome/browser/chromeos/input_method/input_method_manager_impl.h"
#include "chrome/browser/chromeos/input_method/input_method_persistence.h"
#include "ui/base/ime/ime_bridge.h"

namespace chromeos {
namespace input_method {

namespace {
void OnSessionStateChange(InputMethodManagerImpl* input_method_manager_impl,
                          InputMethodPersistence* input_method_persistence,
                          InputMethodManager::UISessionState new_ui_session) {
  input_method_persistence->OnSessionStateChange(new_ui_session);
  input_method_manager_impl->SetUISessionState(new_ui_session);
}

bool g_disable_extension_loading = false;

class InputMethodConfiguration {
 public:
  InputMethodConfiguration() = default;
  virtual ~InputMethodConfiguration() = default;

  void Initialize() {
    ui::IMEBridge::Initialize();

    auto* impl = new InputMethodManagerImpl(
        std::unique_ptr<InputMethodDelegate>(new InputMethodDelegateImpl),
        !g_disable_extension_loading);
    InputMethodManager::Initialize(impl);

    DCHECK(InputMethodManager::Get());

    accessibility_.reset(new Accessibility(impl));
    input_method_persistence_.reset(new InputMethodPersistence(impl));
    browser_state_monitor_.reset(new BrowserStateMonitor(
        base::Bind(&OnSessionStateChange,
                   impl,
                   input_method_persistence_.get())));

    DVLOG(1) << "InputMethodManager initialized";
  }

  void InitializeForTesting(InputMethodManager* mock_manager) {
    InputMethodManager::Initialize(mock_manager);
    DVLOG(1) << "InputMethodManager for testing initialized";
  }

  void Shutdown() {
    accessibility_.reset();
    browser_state_monitor_.reset();
    input_method_persistence_.reset();

    InputMethodManager::Shutdown();

    ui::IMEBridge::Shutdown();

    DVLOG(1) << "InputMethodManager shutdown";
  }

 private:
  std::unique_ptr<Accessibility> accessibility_;
  std::unique_ptr<BrowserStateMonitor> browser_state_monitor_;
  std::unique_ptr<InputMethodPersistence> input_method_persistence_;
};

InputMethodConfiguration* g_input_method_configuration = NULL;

}  // namespace

void Initialize() {
  if (!g_input_method_configuration)
    g_input_method_configuration = new InputMethodConfiguration();
  g_input_method_configuration->Initialize();
}

void InitializeForTesting(InputMethodManager* mock_manager) {
  if (!g_input_method_configuration)
    g_input_method_configuration = new InputMethodConfiguration();
  g_input_method_configuration->InitializeForTesting(mock_manager);
}

void DisableExtensionLoading() {
  g_disable_extension_loading = true;
}

void Shutdown() {
  if (!g_input_method_configuration)
    return;

  g_input_method_configuration->Shutdown();
  delete g_input_method_configuration;
  g_input_method_configuration = NULL;
}

}  // namespace input_method
}  // namespace chromeos
