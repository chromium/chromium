// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_error_ui_android.h"

#include "base/notimplemented.h"

namespace extensions {

ExtensionErrorUIAndroid::ExtensionErrorUIAndroid(
    ExtensionErrorUI::Delegate* delegate) {}

ExtensionErrorUIAndroid::~ExtensionErrorUIAndroid() = default;

bool ExtensionErrorUIAndroid::ShowErrorInBubbleView() {
  // TODO(crbug.com/401047740): Implement for desktop android.
  NOTIMPLEMENTED();

  return false;
}

void ExtensionErrorUIAndroid::ShowExtensions() {
  // TODO(crbug.com/401047740): Implement for desktop android.
  NOTIMPLEMENTED();
}

void ExtensionErrorUIAndroid::Close() {
  // TODO(crbug.com/401047740): Implement for desktop android.
  NOTIMPLEMENTED();
}

}  // namespace extensions
