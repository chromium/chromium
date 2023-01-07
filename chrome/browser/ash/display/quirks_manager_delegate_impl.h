// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DISPLAY_QUIRKS_MANAGER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_DISPLAY_QUIRKS_MANAGER_DELEGATE_IMPL_H_

#include "components/quirks/quirks_manager.h"

namespace quirks {

// Working implementation of QuirksManager::Delegate for access to chrome-
// restricted parts.
class QuirksManagerDelegateImpl : public QuirksManager::Delegate {
 public:
  QuirksManagerDelegateImpl() = default;

  QuirksManagerDelegateImpl(const QuirksManagerDelegateImpl&) = delete;
  QuirksManagerDelegateImpl& operator=(const QuirksManagerDelegateImpl&) =
      delete;

  // QuirksManager::Delegate implementation.
  std::string GetApiKey() const override;
  base::FilePath GetDisplayProfileDirectory() const override;
  bool DevicePolicyEnabled() const override;

 private:
  ~QuirksManagerDelegateImpl() override = default;
};

}  // namespace quirks

#endif  // CHROME_BROWSER_ASH_DISPLAY_QUIRKS_MANAGER_DELEGATE_IMPL_H_
