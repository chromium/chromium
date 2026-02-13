// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_ACCESSIBILITY_ANNOTATOR_DATA_ADAPTER_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_ACCESSIBILITY_ANNOTATOR_DATA_ADAPTER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

class Profile;
class KeyedService;

namespace autofill {

class AccessibilityAnnotatorDataAdapter;

class AutofillAccessibilityAnnotatorDataAdapterFactory
    : public ProfileKeyedServiceFactory {
 public:
  static AccessibilityAnnotatorDataAdapter* GetForProfile(Profile* profile);
  static AutofillAccessibilityAnnotatorDataAdapterFactory* GetInstance();

 private:
  friend base::NoDestructor<AutofillAccessibilityAnnotatorDataAdapterFactory>;

  AutofillAccessibilityAnnotatorDataAdapterFactory();
  ~AutofillAccessibilityAnnotatorDataAdapterFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_ACCESSIBILITY_ANNOTATOR_DATA_ADAPTER_FACTORY_H_
