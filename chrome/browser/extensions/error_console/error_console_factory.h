// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ERROR_CONSOLE_ERROR_CONSOLE_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_ERROR_CONSOLE_ERROR_CONSOLE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {

class ErrorConsole;

class ErrorConsoleFactory : public ProfileKeyedServiceFactory {
 public:
  ErrorConsoleFactory(const ErrorConsoleFactory&) = delete;
  ErrorConsoleFactory& operator=(const ErrorConsoleFactory&) = delete;

  static ErrorConsole* GetForBrowserContext(content::BrowserContext* context);
  static ErrorConsoleFactory* GetInstance();

 private:
  friend base::NoDestructor<ErrorConsoleFactory>;

  ErrorConsoleFactory();
  ~ErrorConsoleFactory() override;

  // BrowserContextKeyedServiceFactory implementation
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ERROR_CONSOLE_ERROR_CONSOLE_FACTORY_H_
