// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_API_GUARD_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_API_GUARD_DELEGATE_H_

#include <memory>
#include <string>

namespace content {
class BrowserContext;
}

namespace chromeos {

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

  ApiGuardDelegate(const ApiGuardDelegate&) = delete;
  ApiGuardDelegate& operator=(const ApiGuardDelegate&) = delete;
  virtual ~ApiGuardDelegate();

  virtual bool IsExtensionForceInstalled(content::BrowserContext* context,
                                         const std::string& extension_id);

 protected:
  ApiGuardDelegate();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_API_GUARD_DELEGATE_H_
