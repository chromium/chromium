// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/test/theme_service_changed_waiter.h"

#include "chrome/browser/themes/theme_service.h"

namespace test {

ThemeServiceChangedWaiter::ThemeServiceChangedWaiter(ThemeService* service)
    : service_(service) {
  DCHECK(service_);
  service_->AddObserver(this);
}

ThemeServiceChangedWaiter::~ThemeServiceChangedWaiter() {
  service_->RemoveObserver(this);
}

void ThemeServiceChangedWaiter::OnThemeChanged() {
  service_->RemoveObserver(this);
  run_loop_.Quit();
}

void ThemeServiceChangedWaiter::WaitForThemeChanged() {
  run_loop_.Run();
}

}  // namespace test
