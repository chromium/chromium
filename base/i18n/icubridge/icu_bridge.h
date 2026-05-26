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

#include "base/files/file_path.h"
#include "base/i18n/base_i18n_export.h"
#include "base/i18n/time_formatting_types.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"

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

  const DateTimeFormatter& date_time_formatter() const {
    return *date_time_formatter_;
  }

 private:
  friend class base::NoDestructor<IcuBridge>;

  IcuBridge();
  ~IcuBridge();

  std::unique_ptr<DateTimeFormatter> date_time_formatter_;
};

}  // namespace base::i18n

#endif  // BASE_I18N_ICUBRIDGE_ICU_BRIDGE_H_
