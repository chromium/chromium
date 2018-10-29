// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_profile_validator_factory.h"

#include <memory>

#include "chrome/browser/autofill/validation_rules_storage_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/libaddressinput/chromium/chrome_metadata_source.h"
#include "third_party/libaddressinput/chromium/chrome_storage_impl.h"

namespace autofill {

// static
AutofillProfileValidator* AutofillProfileValidatorFactory::GetInstance() {
  static base::LazyInstance<AutofillProfileValidatorFactory>::DestructorAtExit
      instance = LAZY_INSTANCE_INITIALIZER;
  return &(instance.Get().autofill_profile_validator_);
}

AutofillProfileValidatorFactory::AutofillProfileValidatorFactory()
    : autofill_profile_validator_(
          std::make_unique<ChromeMetadataSource>(
              I18N_ADDRESS_VALIDATION_DATA_URL,
              g_browser_process->system_network_context_manager()
                  ? g_browser_process->system_network_context_manager()
                        ->GetSharedURLLoaderFactory()
                  : nullptr),
          ValidationRulesStorageFactory::CreateStorage()) {}

AutofillProfileValidatorFactory::~AutofillProfileValidatorFactory() {}

}  // namespace autofill
