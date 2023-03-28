// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_VMM_ARC_SYSTEM_STATE_OBSERVATION_H_
#define CHROME_BROWSER_ASH_ARC_VMM_ARC_SYSTEM_STATE_OBSERVATION_H_

#include "chrome/browser/ash/throttle_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class BrowserContext;
}

namespace arc {

class ArcSystemStateObservation : public ash::ThrottleService {
 public:
  explicit ArcSystemStateObservation(content::BrowserContext* context);

  ArcSystemStateObservation(const ArcSystemStateObservation&) = delete;
  ArcSystemStateObservation& operator=(const ArcSystemStateObservation&) =
      delete;

  ~ArcSystemStateObservation() override;

  absl::optional<base::TimeDelta> GetPeaceDuration();

  base::WeakPtr<ArcSystemStateObservation> GetWeakPtr();

  // ash::ThrottleService override:
  void ThrottleInstance(bool should_throttle) override;

 private:
  absl::optional<base::Time> last_peace_timestamp_;

  base::WeakPtrFactory<ArcSystemStateObservation> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_VMM_ARC_SYSTEM_STATE_OBSERVATION_H_
