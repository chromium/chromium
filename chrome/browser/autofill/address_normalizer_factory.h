// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ADDRESS_NORMALIZER_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_ADDRESS_NORMALIZER_FACTORY_H_

#include "base/lazy_instance.h"
#include "components/autofill/core/browser/address_normalizer_impl.h"

namespace autofill {

// Singleton that owns a single AddressNormalizerImpl instance.
class AddressNormalizerFactory {
 public:
  static AddressNormalizer* GetInstance();

  AddressNormalizerFactory(const AddressNormalizerFactory&) = delete;
  AddressNormalizerFactory& operator=(const AddressNormalizerFactory&) = delete;

 private:
  friend struct base::LazyInstanceTraitsBase<AddressNormalizerFactory>;

  AddressNormalizerFactory();
  ~AddressNormalizerFactory();

  // The only instance that exists.
  AddressNormalizerImpl address_normalizer_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ADDRESS_NORMALIZER_FACTORY_H_
