// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_API_GUARD_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_API_GUARD_DELEGATE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
}

namespace chromeos {

namespace switches {

extern const char kTelemetryExtensionSkipManufacturerCheckForTesting[];

}  // namespace switches

// ApiGuardDelegate is a helper class to offload API guard checks and make it
// test-friendly. E.g. check if the extension is force installed by policy.
class ApiGuardDelegate {
 public:
  class Factory {
   public:
    static std::unique_ptr<ApiGuardDelegate> Create();
    static void SetForTesting(Factory* test_factory);

    virtual ~Factory();

   protected:
    Factory();
    virtual std::unique_ptr<ApiGuardDelegate> CreateInstance() = 0;

   private:
    static Factory* test_factory_;
  };

  // |error_message| represents the result of CanAccessApi(). If |error_message|
  // is sent without a value, the user can access the API; otherwise, the
  // relevant error message is returned.
  using CanAccessApiCallback =
      base::OnceCallback<void(std::optional<std::string> error_message)>;

  ApiGuardDelegate(const ApiGuardDelegate&) = delete;
  ApiGuardDelegate& operator=(const ApiGuardDelegate&) = delete;
  virtual ~ApiGuardDelegate();

  virtual void CanAccessApi(content::BrowserContext* context,
                            const extensions::Extension* extension,
                            CanAccessApiCallback callback) = 0;

 protected:
  ApiGuardDelegate();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_API_GUARD_DELEGATE_H_
