// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/input_method_configuration.h"

#include <memory>

#include "chrome/browser/ash/input_method/accessibility.h"
#include "chrome/browser/ash/input_method/component_extension_ime_manager_delegate_impl.h"
#include "chrome/browser/ash/input_method/input_method_delegate_impl.h"
#include "chrome/browser/ash/input_method/input_method_manager_impl.h"
#include "chrome/browser/ash/input_method/input_method_persistence.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace ash {
namespace input_method {

namespace {

bool g_disable_extension_loading = false;
Accessibility* g_accessibility = nullptr;
InputMethodPersistence* g_input_method_persistence = nullptr;

}  // namespace

void Initialize() {
  auto* impl = new InputMethodManagerImpl(
      std::make_unique<InputMethodDelegateImpl>(),
      std::make_unique<ComponentExtensionIMEManagerDelegateImpl>(),
      !g_disable_extension_loading);
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
