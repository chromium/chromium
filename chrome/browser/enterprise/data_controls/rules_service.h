// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_RULES_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_RULES_SERVICE_H_

#include "chrome/browser/enterprise/data_controls/chrome_dlp_rules_manager.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/enterprise/data_controls/verdict.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace data_controls {

// Keyed service that provides an interface to check what restrictions should
// be applied from the DataControlsRules policy.
class RulesService : public KeyedService {
 public:
  ~RulesService() override;

  Verdict GetPrintVerdict(const GURL& printed_page_url) const;

  // TODO(b/307291932): Once crrev.com/c/5054488 lands, implement this method.
  // Verdict GetPasteVerdict(
  //     const content::ClipboardEndpoint& source,
  //     const content::ClipboardEndpoint& destination,
  //     const content::ClipboardMetadata& metadata);

 protected:
  friend class RulesServiceFactory;

  explicit RulesService(content::BrowserContext* browser_context);

 private:
  // Initialized with the profile passed in the constructor.
  ChromeDlpRulesManager rules_manager_;
};

class RulesServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static RulesService* GetForBrowserContext(content::BrowserContext* context);

  static RulesServiceFactory* GetInstance();

  RulesServiceFactory(const RulesServiceFactory&) = delete;
  RulesServiceFactory& operator=(const RulesServiceFactory&) = delete;

 private:
  friend base::NoDestructor<RulesServiceFactory>;

  RulesServiceFactory();
  ~RulesServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace data_controls

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_RULES_SERVICE_H_
