// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/chrome_linux_ui_getter.h"

// static
ChromeLinuxUiGetter* ChromeLinuxUiGetter::instance_ = nullptr;

ChromeLinuxUiGetter::ChromeLinuxUiGetter() {
  instance_ = this;
}

ChromeLinuxUiGetter::~ChromeLinuxUiGetter() {
  if (instance_ == this) {
    instance_ = nullptr;
  }
}
