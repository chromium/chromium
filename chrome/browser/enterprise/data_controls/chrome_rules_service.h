// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_CHROME_RULES_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_CHROME_RULES_SERVICE_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/enterprise/data_controls/content/browser/rules_service.h"
#include "components/enterprise/data_controls/content/browser/rules_service_factory.h"
#include "components/enterprise/data_controls/core/browser/verdict.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/clipboard_types.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace data_controls {

// Desktop-specific implementation of `data_controls::RulesService`.
class ChromeRulesService : public RulesService {
 public:
  ~ChromeRulesService() override;

  // data_controls::RulesService:
  Verdict GetPrintVerdict(const GURL& printed_page_url) const override;
  Verdict GetPasteVerdict(
      const content::ClipboardEndpoint& source,
      const content::ClipboardEndpoint& destination,
      const content::ClipboardMetadata& metadata) const override;
  Verdict GetCopyRestrictedBySourceVerdict(const GURL& source) const override;
  Verdict GetCopyToOSClipboardVerdict(const GURL& source) const override;
  bool BlockScreenshots(const GURL& url) const override;

 protected:
  friend class ChromeRulesServiceFactory;

  explicit ChromeRulesService(content::BrowserContext* browser_context);

 private:
  // Helpers to convert action-specific types to rule-specific types.
  ActionSource GetAsActionSource(
      const content::ClipboardEndpoint& endpoint) const;
  ActionDestination GetAsActionDestination(
      const content::ClipboardEndpoint& endpoint) const;
  template <typename ActionSourceOrDestination>
  ActionSourceOrDestination ExtractPasteActionContext(
      const content::ClipboardEndpoint& endpoint) const;

  // `profile_` and `rules_manager_` are initialized with the browser_context
  // passed in the constructor.
  const raw_ptr<Profile> profile_ = nullptr;

  // Watches changes to the "DataControlsRules" policy. Does nothing if the
  // "EnableDesktopDataControls" experiment is disabled.
  PrefChangeRegistrar pref_registrar_;

  // List of rules created from the "DataControlsRules" policy. Empty if the
  // "EnableDesktopDataControls" experiment is disabled.
  std::vector<Rule> rules_;
};

class ChromeRulesServiceFactory : public RulesServiceFactory,
                                  public ProfileKeyedServiceFactory {
 public:
  // data_controls::RulesServiceFactory:
  RulesService* GetForBrowserContext(content::BrowserContext* context) override;

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
