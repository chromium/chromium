// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_CHROME_RULES_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_CHROME_RULES_SERVICE_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/enterprise/data_controls/core/browser/rules_service_base.h"
#include "components/enterprise/data_controls/core/browser/verdict.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/clipboard_types.h"
#include "ui/base/clipboard/clipboard_metadata.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace data_controls {

// Desktop-specific implementation of `data_controls::RulesServiceBase`.
class ChromeRulesService : public RulesServiceBase {
 public:
  ~ChromeRulesService() override;

  Verdict GetPrintVerdict(const GURL& printed_page_url) const;

  // Returns a clipboard verdict to be applied to a paste action. A null browser
  // context on `source` represents data coming from the OS clipboard.
  // `destination` is always expected to have a valid browser context.
  Verdict GetPasteVerdict(const content::ClipboardEndpoint& source,
                          const content::ClipboardEndpoint& destination) const;

  // Returns true if rules indicate screenshots should be blocked. Only the
  // "block" level is supported, a "warn" screenshot rule will not make this
  // functions return true.
  bool BlockScreenshots(const GURL& url) const;

 protected:
  friend class ChromeRulesServiceFactory;

  explicit ChromeRulesService(content::BrowserContext* browser_context);

 private:
  // RulesServiceBase:
  bool incognito_profile() const override;

  // Helpers to convert action-specific types to rule-specific types.
  ActionSource GetAsActionSource(
      const content::ClipboardEndpoint& endpoint) const;
  ActionDestination GetAsActionDestination(
      const content::ClipboardEndpoint& endpoint) const;
  template <typename ActionSourceOrDestination>
  ActionSourceOrDestination ExtractPasteActionContext(
      const content::ClipboardEndpoint& endpoint) const;

  // Initialized with the browser_context passed in the constructor.
  const raw_ptr<Profile> profile_ = nullptr;
};

class ChromeRulesServiceFactory : public ProfileKeyedServiceFactory {
 public:
  ChromeRulesService* GetForBrowserContext(content::BrowserContext* context);

  static ChromeRulesServiceFactory* GetInstance();

  ChromeRulesServiceFactory(const ChromeRulesServiceFactory&) = delete;
  ChromeRulesServiceFactory& operator=(const ChromeRulesServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<ChromeRulesServiceFactory>;

  ChromeRulesServiceFactory();
  ~ChromeRulesServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace data_controls

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_CHROME_RULES_SERVICE_H_
