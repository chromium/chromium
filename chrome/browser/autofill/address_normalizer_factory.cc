// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/address_normalizer_factory.h"

#include <memory>

#include "chrome/browser/autofill/validation_rules_storage_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/libaddressinput/chromium/chrome_metadata_source.h"
#include "third_party/libaddressinput/chromium/chrome_storage_impl.h"

namespace autofill {

// static
AddressNormalizer* AddressNormalizerFactory::GetInstance() {
  static base::LazyInstance<AddressNormalizerFactory>::DestructorAtExit
      instance = LAZY_INSTANCE_INITIALIZER;
  return &(instance.Get().address_normalizer_);
}

AddressNormalizerFactory::AddressNormalizerFactory()
    : address_normalizer_(
          std::unique_ptr<::i18n::addressinput::Source>(
              std::make_unique<ChromeMetadataSource>(
                  I18N_ADDRESS_VALIDATION_DATA_URL,
                  g_browser_process->system_network_context_manager()
                      ? g_browser_process->system_network_context_manager()
                            ->GetSharedURLLoaderFactory()
                      : nullptr)),
          ValidationRulesStorageFactory::CreateStorage(),
          g_browser_process->GetApplicationLocale()) {}

AddressNormalizerFactory::~AddressNormalizerFactory() {}

}  // namespace autofill
