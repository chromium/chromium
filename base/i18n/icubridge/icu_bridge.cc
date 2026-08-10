// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icubridge/icu_bridge.h"

#include "base/feature_list.h"
#include "base/i18n/icubridge/calendar.h"
#include "base/i18n/icubridge/date_time_formatter.h"
#include "base/i18n/icubridge/features.h"
#include "base/i18n/icubridge/normalizer.h"
#include "base/no_destructor.h"

namespace base::i18n {

// static
IcuBridge& IcuBridge::GetInstance() {
  static base::NoDestructor<IcuBridge> instance;
  return *instance;
}

IcuBridge::IcuBridge()
    : date_time_formatter_(
          std::make_unique<DateTimeFormatter>(base::PassKey<IcuBridge>())),
      calendar_(std::make_unique<Calendar>(base::PassKey<IcuBridge>())),
      icu4x_normalizer_(CreateIcu4xNormalizer(base::PassKey<IcuBridge>())),
      icu4c_normalizer_(CreateIcu4cNormalizer(base::PassKey<IcuBridge>())) {}

IcuBridge::~IcuBridge() = default;

const IcuBridge::Normalizer& IcuBridge::normalizer() const {
  return base::FeatureList::IsEnabled(kUseIcu4xNormalizer) ? *icu4x_normalizer_
                                                           : *icu4c_normalizer_;
}

}  // namespace base::i18n
