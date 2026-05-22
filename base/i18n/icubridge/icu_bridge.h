// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_ICUBRIDGE_ICU_BRIDGE_H_
#define BASE_I18N_ICUBRIDGE_ICU_BRIDGE_H_

#include "base/i18n/base_i18n_export.h"
#include "base/no_destructor.h"

namespace base::i18n {

// IcuBridge is a container for specialized internationalization components.
// It is designed to be modular, allowing each component to potentially
// use different backends in the future.
class BASE_I18N_EXPORT IcuBridge {
 public:
  static IcuBridge& GetInstance();

  IcuBridge(const IcuBridge&) = delete;
  IcuBridge& operator=(const IcuBridge&) = delete;

 private:
  friend class base::NoDestructor<IcuBridge>;

  IcuBridge();
  ~IcuBridge();
};

}  // namespace base::i18n

#endif  // BASE_I18N_ICUBRIDGE_ICU_BRIDGE_H_
