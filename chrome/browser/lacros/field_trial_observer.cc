// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/field_trial_observer.h"

#include "base/strings/strcat.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/variations/active_field_trials.h"

namespace {
// Prefix prepended by Lacros before sending ash field trials as
// synthetic field trials.
constexpr char ASH_FIELD_TRIAL_PREFIX[] = "ASH_";
}  // namespace

FieldTrialObserver::FieldTrialObserver() = default;
FieldTrialObserver::~FieldTrialObserver() = default;

void FieldTrialObserver::Start() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::FieldTrialService>())
    return;

  // Check if Ash is too old to support FieldTrialObserver.
  int version =
      lacros_service->GetInterfaceVersion<crosapi::mojom::FieldTrialService>();
  int min_required_version =
      static_cast<int>(crosapi::mojom::FieldTrialService::MethodMinVersions::
                           kAddFieldTrialObserverMinVersion);
  if (version < min_required_version)
    return;

  lacros_service->GetRemote<crosapi::mojom::FieldTrialService>()
      ->AddFieldTrialObserver(receiver_.BindNewPipeAndPassRemote());
}

void FieldTrialObserver::OnFieldTrialGroupActivated(
    std::vector<crosapi::mojom::FieldTrialGroupInfoPtr> infos) {
  for (const auto& info : infos) {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        ASH_FIELD_TRIAL_PREFIX + info->trial_name,
        info->is_overridden.value_or(false)
            ? base::StrCat({info->group_name, variations::kOverrideSuffix})
            : info->group_name);
  }
}
