// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icubridge/icu_bridge.h"

#include "base/i18n/icubridge/date_time_formatter.h"
#include "base/no_destructor.h"

namespace base::i18n {

// static
IcuBridge& IcuBridge::GetInstance() {
  static base::NoDestructor<IcuBridge> instance;
  return *instance;
}

IcuBridge::IcuBridge()
    : date_time_formatter_(
          std::make_unique<DateTimeFormatter>(base::PassKey<IcuBridge>())) {}
IcuBridge::~IcuBridge() = default;

}  // namespace base::i18n
