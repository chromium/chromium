// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_LACROS_CHROME_SERVICE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_LACROS_LACROS_CHROME_SERVICE_DELEGATE_IMPL_H_

#include "chromeos/lacros/lacros_chrome_service_delegate.h"

// Chrome implementation of LacrosChromeServiceDelegate.
class LacrosChromeServiceDelegateImpl
    : public chromeos::LacrosChromeServiceDelegate {
 public:
  LacrosChromeServiceDelegateImpl();
  LacrosChromeServiceDelegateImpl(const LacrosChromeServiceDelegateImpl&) =
      delete;
  LacrosChromeServiceDelegateImpl& operator=(
      const LacrosChromeServiceDelegateImpl&) = delete;
  ~LacrosChromeServiceDelegateImpl() override;

  // chromeos::LacrosChromeServiceDelegate:
  void NewWindow() override;
  std::string GetChromeVersion() override;
};

#endif  // CHROME_BROWSER_LACROS_LACROS_CHROME_SERVICE_DELEGATE_IMPL_H_
