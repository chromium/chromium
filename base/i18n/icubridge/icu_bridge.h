// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_ICUBRIDGE_ICU_BRIDGE_H_
#define BASE_I18N_ICUBRIDGE_ICU_BRIDGE_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/containers/lru_cache.h"
#include "base/files/file_path.h"
#include "base/i18n/base_i18n_export.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/time_formatting_types.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace base::i18n {

// IcuBridge is a container for specialized internationalization components.
// It is designed to be modular, allowing each component to potentially
// use different backends in the future.
class BASE_I18N_EXPORT IcuBridge {
 public:
  static IcuBridge& GetInstance();

  IcuBridge(const IcuBridge&) = delete;
  IcuBridge& operator=(const IcuBridge&) = delete;

  class BASE_I18N_EXPORT DateTimeFormatter;
  class BASE_I18N_EXPORT Calendar;
  class BASE_I18N_EXPORT ListFormatter;
  class BASE_I18N_EXPORT Normalizer;

  // Returns the formatter for the current default ICU locale, see
  // `GetDefaultIcuLocale()`.
  const DateTimeFormatter& date_time_formatter() const;

  // Returns the formatter for `locale`. Formatters are created on first use and
  // cached for the lifetime of the process.
  const DateTimeFormatter& date_time_formatter(const LanguageTag& locale) const;

  const Calendar& calendar() const { return *calendar_; }

  // Returns the list formatter for the current default ICU locale, see
  // `GetDefaultIcuLocale()`.
  scoped_refptr<ListFormatter> list_formatter() const;

  // Returns the list formatter for `locale`. Formatters are created on first
  // use and the most recently used ones are cached.
  scoped_refptr<ListFormatter> list_formatter(const LanguageTag& locale) const;

  const Normalizer& normalizer() const;

 private:
  friend class base::NoDestructor<IcuBridge>;

  IcuBridge();
  ~IcuBridge();

  // `date_time_formatters_` is lazily populated by the const
  // `date_time_formatter()` accessors, which may be called from any thread.
  mutable base::Lock date_time_formatters_lock_;
  mutable absl::flat_hash_map<LanguageTag, std::unique_ptr<DateTimeFormatter>>
      date_time_formatters_ GUARDED_BY(date_time_formatters_lock_);
  // `list_formatters_` is lazily populated by the const `list_formatter()`
  // accessors, which may be called from any thread. Every access must hold
  // `list_formatters_lock_`, including lookups, since `LRUCache::Get()` updates
  // the recency order. Evicting a formatter that a caller still holds is safe
  // because `ListFormatter` is ref-counted.
  mutable base::Lock list_formatters_lock_;
  mutable base::LRUCache<LanguageTag, scoped_refptr<ListFormatter>>
      list_formatters_ GUARDED_BY(list_formatters_lock_);
  std::unique_ptr<Calendar> calendar_;
  std::unique_ptr<Normalizer> icu4x_normalizer_;
  std::unique_ptr<Normalizer> icu4c_normalizer_;
};

}  // namespace base::i18n

#endif  // BASE_I18N_ICUBRIDGE_ICU_BRIDGE_H_
