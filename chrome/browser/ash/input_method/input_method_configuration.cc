// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/input_method_configuration.h"

#include <memory>

#include "base/system/sys_info.h"
#include "chrome/browser/ash/input_method/accessibility.h"
#include "chrome/browser/ash/input_method/component_extension_ime_manager_delegate_impl.h"
#include "chrome/browser/ash/input_method/input_method_delegate_impl.h"
#include "chrome/browser/ash/input_method/input_method_manager_impl.h"
#include "chrome/browser/ash/input_method/input_method_persistence.h"
#include "ui/base/ime/ash/fake_ime_keyboard.h"
#include "ui/base/ime/ash/ime_keyboard_impl.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ash {
namespace input_method {

namespace {

bool g_disable_extension_loading = false;
Accessibility* g_accessibility = nullptr;
InputMethodPersistence* g_input_method_persistence = nullptr;

}  // namespace

void Initialize() {
  std::unique_ptr<ImeKeyboard> ime_keyboard;
  if (base::SysInfo::IsRunningOnChromeOS()) {
    ime_keyboard = std::make_unique<ImeKeyboardImpl>(
        ui::OzonePlatform::GetInstance()->GetInputController());
  } else {
    ime_keyboard = std::make_unique<FakeImeKeyboard>();
  }

  auto* impl = new InputMethodManagerImpl(
      std::make_unique<InputMethodDelegateImpl>(),
      std::make_unique<ComponentExtensionIMEManagerDelegateImpl>(),
      !g_disable_extension_loading, std::move(ime_keyboard));
  InputMethodManager::Initialize(impl);
  DCHECK(InputMethodManager::Get());

  delete g_accessibility;
  g_accessibility = new Accessibility(impl);

  delete g_input_method_persistence;
  g_input_method_persistence = new InputMethodPersistence(impl);
}

void InitializeForTesting(InputMethodManager* mock_manager) {
  InputMethodManager::Initialize(mock_manager);
}

void DisableExtensionLoading() {
  g_disable_extension_loading = true;
}

void Shutdown() {
  delete g_accessibility;
  g_accessibility = nullptr;

  delete g_input_method_persistence;
  g_input_method_persistence = nullptr;

  InputMethodManager::Shutdown();
}

}  // namespace input_method
}  // namespace ash
