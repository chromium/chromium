// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_accessibility_annotator_data_adapter_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/accessibility_annotator_data_adapter.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/keyed_service/core/keyed_service.h"

namespace autofill {

// static
AccessibilityAnnotatorDataAdapter*
AutofillAccessibilityAnnotatorDataAdapterFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AccessibilityAnnotatorDataAdapter*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
AutofillAccessibilityAnnotatorDataAdapterFactory*
AutofillAccessibilityAnnotatorDataAdapterFactory::GetInstance() {
  static base::NoDestructor<AutofillAccessibilityAnnotatorDataAdapterFactory>
      instance;
  return instance.get();
}

AutofillAccessibilityAnnotatorDataAdapterFactory::
    AutofillAccessibilityAnnotatorDataAdapterFactory()
    : ProfileKeyedServiceFactory(
          "AccessibilityAnnotatorDataAdapter",
          ProfileSelections::BuildRedirectedInIncognito()) {}

AutofillAccessibilityAnnotatorDataAdapterFactory::
    ~AutofillAccessibilityAnnotatorDataAdapterFactory() = default;

std::unique_ptr<KeyedService> AutofillAccessibilityAnnotatorDataAdapterFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillUseAccessibilityAnnotator)) {
    return nullptr;
  }

  return std::make_unique<AccessibilityAnnotatorDataAdapter>();
}

}  // namespace autofill
