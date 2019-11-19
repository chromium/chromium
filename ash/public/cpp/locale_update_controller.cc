// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/locale_update_controller.h"

#include "base/logging.h"

namespace ash {

namespace {

LocaleUpdateController* g_instance = nullptr;

}  // namespace

LocaleInfo::LocaleInfo() = default;
LocaleInfo::LocaleInfo(const std::string& iso_code,
                       const base::string16& display_name)
    : iso_code(iso_code), display_name(display_name) {}
LocaleInfo::LocaleInfo(const LocaleInfo& rhs) = default;
LocaleInfo::LocaleInfo(LocaleInfo&& rhs) = default;
LocaleInfo::~LocaleInfo() = default;

// static
LocaleUpdateController* LocaleUpdateController::Get() {
  return g_instance;
}

LocaleUpdateController::LocaleUpdateController() {
  DCHECK(!g_instance);
  g_instance = this;
}

LocaleUpdateController::~LocaleUpdateController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
