// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ACTOR_ACTOR_FORM_FILLING_SERVICE_IMPL_TEST_API_H_
#define CHROME_BROWSER_AUTOFILL_ACTOR_ACTOR_FORM_FILLING_SERVICE_IMPL_TEST_API_H_

#include <vector>

#include "chrome/browser/autofill/actor/actor_form_filling_service_impl.h"

namespace autofill {

// Helper class to simplify testing `ActorFormFillingServiceImpl`.
class ActorFormFillingServiceImplTestApi {
 public:
  explicit ActorFormFillingServiceImplTestApi(
      ActorFormFillingServiceImpl& service)
      : service_(service) {}

  std::vector<ActorFormFillingError> FillingErrors() {
    return service_->errors_per_session_;
  }

 private:
  raw_ref<ActorFormFillingServiceImpl> service_;
};

inline ActorFormFillingServiceImplTestApi test_api(
    ActorFormFillingServiceImpl& service) {
  return ActorFormFillingServiceImplTestApi(service);
}

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ACTOR_ACTOR_FORM_FILLING_SERVICE_IMPL_TEST_API_H_
