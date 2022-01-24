// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_PROFILE_VALIDATOR_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_PROFILE_VALIDATOR_FACTORY_H_

#include "base/lazy_instance.h"
#include "components/autofill/core/browser/autofill_profile_validator.h"

namespace autofill {

// Singleton that owns a single AutofillProfileValidator instance.
class AutofillProfileValidatorFactory {
 public:
  static AutofillProfileValidator* GetInstance();

  AutofillProfileValidatorFactory(const AutofillProfileValidatorFactory&) =
      delete;
  AutofillProfileValidatorFactory& operator=(
      const AutofillProfileValidatorFactory&) = delete;

 private:
  friend struct base::LazyInstanceTraitsBase<AutofillProfileValidatorFactory>;

  AutofillProfileValidatorFactory();
  ~AutofillProfileValidatorFactory();

  // The only instance that exists.
  AutofillProfileValidator autofill_profile_validator_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_PROFILE_VALIDATOR_FACTORY_H_
