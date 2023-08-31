// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_SUBKEY_REQUESTER_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_SUBKEY_REQUESTER_FACTORY_H_

#include "base/lazy_instance.h"
#include "components/autofill/core/browser/geo/subkey_requester.h"

namespace autofill {

// Singleton that owns a single SubKeyRequester instance.
class SubKeyRequesterFactory {
 public:
  static SubKeyRequester* GetInstance();

  SubKeyRequesterFactory(const SubKeyRequesterFactory&) = delete;
  SubKeyRequesterFactory& operator=(const SubKeyRequesterFactory&) = delete;

 private:
  friend struct base::LazyInstanceTraitsBase<SubKeyRequesterFactory>;

  SubKeyRequesterFactory();
  ~SubKeyRequesterFactory();

  // The only instance that exists.
  SubKeyRequester subkey_requester_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_SUBKEY_REQUESTER_FACTORY_H_
