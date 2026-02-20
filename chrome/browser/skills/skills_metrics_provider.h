// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SKILLS_SKILLS_METRICS_PROVIDER_H_
#define CHROME_BROWSER_SKILLS_SKILLS_METRICS_PROVIDER_H_

#include <vector>

#include "base/functional/callback.h"
#include "components/metrics/metrics_provider.h"

class Profile;

namespace skills {

// A registerable metrics provider that emits the number of skills a user has
// per profile upon UMA upload.
class SkillsMetricsProvider : public metrics::MetricsProvider {
 public:
  using ProfileListCallback = base::RepeatingCallback<std::vector<Profile*>()>;

  explicit SkillsMetricsProvider(ProfileListCallback profile_list_callback);
  ~SkillsMetricsProvider() override;

  // metrics::MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  ProfileListCallback profile_list_callback_;
};

}  // namespace skills

#endif  // CHROME_BROWSER_SKILLS_SKILLS_METRICS_PROVIDER_H_
