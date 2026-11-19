// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icubridge/icu_bridge.h"

#include <stddef.h>

#include <memory>

#include "base/containers/lru_cache.h"
#include "base/feature_list.h"
#include "base/i18n/icubridge/calendar.h"
#include "base/i18n/icubridge/date_time_formatter.h"
#include "base/i18n/icubridge/default_icu_locale.h"
#include "base/i18n/icubridge/features.h"
#include "base/i18n/icubridge/list_formatter.h"
#include "base/i18n/icubridge/normalizer.h"
#include "base/i18n/language_tag.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"

namespace base::i18n {

namespace {

// Maximum number of list formatters kept in `IcuBridge::list_formatters_`.
// Most processes only use a few locales, so a small cache bounds memory usage
// while keeping the formatters of the locales in use.
constexpr size_t kListFormatterCacheCapacity = 8;

}  // namespace

// static
IcuBridge& IcuBridge::GetInstance() {
  static base::NoDestructor<IcuBridge> instance;
  return *instance;
}

IcuBridge::IcuBridge()
    : list_formatters_(kListFormatterCacheCapacity),
      calendar_(std::make_unique<Calendar>(base::PassKey<IcuBridge>())),
      icu4x_normalizer_(CreateIcu4xNormalizer(base::PassKey<IcuBridge>())),
      icu4c_normalizer_(CreateIcu4cNormalizer(base::PassKey<IcuBridge>())) {}

IcuBridge::~IcuBridge() = default;

const IcuBridge::DateTimeFormatter& IcuBridge::date_time_formatter() const {
  return date_time_formatter(GetDefaultIcuLocale());
}

const IcuBridge::DateTimeFormatter& IcuBridge::date_time_formatter(
    const LanguageTag& locale) const {
  base::AutoLock auto_lock(date_time_formatters_lock_);
  std::unique_ptr<DateTimeFormatter>& formatter = date_time_formatters_[locale];
  if (!formatter) {
    formatter =
        std::make_unique<DateTimeFormatter>(base::PassKey<IcuBridge>(), locale);
  }
  return *formatter;
}

scoped_refptr<IcuBridge::ListFormatter> IcuBridge::list_formatter() const {
  return list_formatter(GetDefaultIcuLocale());
}

scoped_refptr<IcuBridge::ListFormatter> IcuBridge::list_formatter(
    const LanguageTag& locale) const {
  base::AutoLock auto_lock(list_formatters_lock_);
  if (auto it = list_formatters_.Get(locale); it != list_formatters_.end()) {
    return it->second;
  }
  scoped_refptr<ListFormatter> formatter =
      base::MakeRefCounted<ListFormatter>(base::PassKey<IcuBridge>(), locale);
  list_formatters_.Put(locale, formatter);
  return formatter;
}

const IcuBridge::Normalizer& IcuBridge::normalizer() const {
  return base::FeatureList::IsEnabled(kUseIcu4xNormalizer) ? *icu4x_normalizer_
                                                           : *icu4c_normalizer_;
}

}  // namespace base::i18n
