// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_VALIDATION_RULES_STORAGE_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_VALIDATION_RULES_STORAGE_FACTORY_H_

#include <memory>

#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"

namespace i18n {
namespace addressinput {
class Storage;
}
}

class JsonPrefStore;

namespace autofill {

// Creates Storage objects, all of which are backed by a common pref store.
class ValidationRulesStorageFactory {
 public:
  static std::unique_ptr<::i18n::addressinput::Storage> CreateStorage();

  ValidationRulesStorageFactory(const ValidationRulesStorageFactory&) = delete;
  ValidationRulesStorageFactory& operator=(
      const ValidationRulesStorageFactory&) = delete;

 private:
  friend struct base::LazyInstanceTraitsBase<ValidationRulesStorageFactory>;

  ValidationRulesStorageFactory();
  ~ValidationRulesStorageFactory();

  scoped_refptr<JsonPrefStore> json_pref_store_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_VALIDATION_RULES_STORAGE_FACTORY_H_
