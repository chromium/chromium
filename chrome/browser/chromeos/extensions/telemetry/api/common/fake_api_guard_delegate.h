// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_FAKE_API_GUARD_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_FAKE_API_GUARD_DELEGATE_H_

#include <memory>
#include <string>

#include "chrome/browser/chromeos/extensions/telemetry/api/common/api_guard_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
}

namespace chromeos {

class FakeApiGuardDelegate : public ApiGuardDelegate {
 public:
  class Factory : public ApiGuardDelegate::Factory {
   public:
    explicit Factory(absl::optional<std::string> error_message);
    ~Factory() override;

   protected:
    // ApiGuardDelegate::Factory:
    std::unique_ptr<ApiGuardDelegate> CreateInstance() override;

   private:
    absl::optional<std::string> error_message_;
  };

  FakeApiGuardDelegate(const FakeApiGuardDelegate&) = delete;
  FakeApiGuardDelegate& operator=(const FakeApiGuardDelegate&) = delete;
  ~FakeApiGuardDelegate() override;

  // ApiGuardDelegate:
  void CanAccessApi(content::BrowserContext* context,
                    const extensions::Extension* extension,
                    CanAccessApiCallback callback) override;

 protected:
  explicit FakeApiGuardDelegate(absl::optional<std::string> error_message);

 private:
  // Error message returned when calling CanAccessApi().
  absl::optional<std::string> error_message_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_FAKE_API_GUARD_DELEGATE_H_
