// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEDERATED_LEARNING_FLOC_ID_PROVIDER_H_
#define CHROME_BROWSER_FEDERATED_LEARNING_FLOC_ID_PROVIDER_H_

#include "components/federated_learning/floc_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/cookies/site_for_cookies.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/federated_learning/floc.mojom-forward.h"
#include "url/origin.h"

namespace federated_learning {

// KeyedService which computes the floc id regularly, and notifies relevant
// components about the updated id. The floc id represents a cohort of people
// with similar browsing habits. For more context, see the explainer at
// https://github.com/jkarlin/floc/blob/master/README.md.
class FlocIdProvider : public KeyedService {
 public:
  // Get the interest cohort in a particular context. Use the requesting
  // context's |url| and the first-party context |top_frame_origin| for the
  // access permission check.
  virtual blink::mojom::InterestCohortPtr GetInterestCohortForJsApi(
      const GURL& url,
      const base::Optional<url::Origin>& top_frame_origin) const = 0;

  // Record the floc id to UKM if this is the first recording attempt after each
  // time the floc is (re-)computed. No-op if the existing floc was already
  // recorded to UKM before.
  virtual void MaybeRecordFlocToUkm(ukm::SourceId source_id) = 0;

  ~FlocIdProvider() override = default;
};

}  // namespace federated_learning

#endif  // CHROME_BROWSER_FEDERATED_LEARNING_FLOC_ID_PROVIDER_H_
