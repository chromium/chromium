// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_FAKE_API_GUARD_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_FAKE_API_GUARD_DELEGATE_H_

#include <memory>
#include <string>

#include "chrome/browser/chromeos/extensions/telemetry/api/api_guard_delegate.h"

namespace content {
class BrowserContext;
}

namespace chromeos {

class FakeApiGuardDelegate : public ApiGuardDelegate {
 public:
  class Factory : public ApiGuardDelegate::Factory {
   public:
    explicit Factory(bool is_extension_force_installed);
    ~Factory() override;

   protected:
    // ApiGuardDelegate::Factory:
    std::unique_ptr<ApiGuardDelegate> CreateInstance() override;

   private:
    bool is_extension_force_installed_;
  };

  FakeApiGuardDelegate(const FakeApiGuardDelegate&) = delete;
  FakeApiGuardDelegate& operator=(const FakeApiGuardDelegate&) = delete;
  ~FakeApiGuardDelegate() override;

  // ApiGuardDelegate:
  bool IsExtensionForceInstalled(content::BrowserContext* context,
                                 const std::string& extension_id) override;

 protected:
  explicit FakeApiGuardDelegate(bool is_extension_force_installed);

 private:
  bool is_extension_force_installed_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_FAKE_API_GUARD_DELEGATE_H_
