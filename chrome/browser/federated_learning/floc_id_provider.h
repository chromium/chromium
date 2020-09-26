// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEDERATED_LEARNING_FLOC_ID_PROVIDER_H_
#define CHROME_BROWSER_FEDERATED_LEARNING_FLOC_ID_PROVIDER_H_

#include "components/federated_learning/floc_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/cookies/site_for_cookies.h"
#include "url/origin.h"

namespace federated_learning {

// KeyedService which computes the floc id regularly, and notifies relevant
// components about the updated id.
class FlocIdProvider : public KeyedService {
 public:
  // Get the interest cohort. Use |requesting_origin| and first-party
  // context |site_for_cookies| for access permission check.
  virtual std::string GetInterestCohortForJsApi(
      const url::Origin& requesting_origin,
      const net::SiteForCookies& site_for_cookies) const = 0;

  ~FlocIdProvider() override = default;
};

}  // namespace federated_learning

#endif  // CHROME_BROWSER_FEDERATED_LEARNING_FLOC_ID_PROVIDER_H_
