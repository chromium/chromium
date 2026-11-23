// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_assistant_switcher.h"

#include <string_view>
#include <utility>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chrome/browser/glic/common/glic_relaunch.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_change.h"
#include "chrome/browser/glic/public/glic_provider_metrics.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/glic/public/glic_provider_switch_notice.h"
#include "chrome/browser/profiles/profile.h"

namespace glic {

namespace {

// LINT.IfChange(GlicAssistantTargetSuffix)
std::string_view GetAssistantTargetHistogramSuffix(
    mojom::AssistantTarget target) {
  switch (target) {
    case mojom::AssistantTarget::kUnknown:
      return "Unknown";
    case mojom::AssistantTarget::kGeminiConsumer:
      return "GeminiConsumer";
    case mojom::AssistantTarget::kGeminiEnterprise:
      return "GeminiEnterprise";
  }
  NOTREACHED();
}
// LINT.ThenChange(//chrome/browser/glic/host/glic.mojom:AssistantTarget,
// //tools/metrics/histograms/metadata/glic/histograms.xml:GlicAssistantTarget)

}  // namespace

bool CanSwitchToAnyAssistant(Profile& profile) {
  return CanSwitchAssistant(profile, mojom::AssistantTarget::kGeminiConsumer) ||
         CanSwitchAssistant(profile, mojom::AssistantTarget::kGeminiEnterprise);
}

bool CanSwitchAssistant(Profile& profile, mojom::AssistantTarget target) {
  if (profile.IsOffTheRecord() || !profile.GetPrefs()) {
    return false;
  }
  switch (target) {
    case mojom::AssistantTarget::kUnknown:
      return false;
    case mojom::AssistantTarget::kGeminiConsumer:
      return GetGlicProviderSpec(&profile).CanSwitchToProvider(
          profile, GlicProvider::kGemini);
    case mojom::AssistantTarget::kGeminiEnterprise:
      return GetGlicProviderSpec(&profile).CanSwitchToProvider(
          profile, GlicProvider::kGeminiEnterprise);
  }
}

void SwitchAssistant(
    Profile& profile,
    mojom::AssistantTarget target,
    base::OnceCallback<void(mojom::SwitchAssistantResponsePtr)> callback) {
  auto response = mojom::SwitchAssistantResponse::New(
      mojom::SwitchAssistantResult::kUnknown);
  if (target == mojom::AssistantTarget::kUnknown) {
    response->result = mojom::SwitchAssistantResult::kErrorInvalidTarget;
  } else if (!CanSwitchAssistant(profile, target)) {
    response->result = mojom::SwitchAssistantResult::kErrorNotAllowed;
  } else {
    switch (target) {
      case mojom::AssistantTarget::kGeminiConsumer:
        SwitchAssistantSelection(profile, prefs::AssistantSelection::kConsumer);
        break;
      case mojom::AssistantTarget::kGeminiEnterprise:
        SwitchAssistantSelection(profile,
                                 prefs::AssistantSelection::kEnterprise);
        break;
      case mojom::AssistantTarget::kUnknown:
        NOTREACHED();
    }
    response->result = mojom::SwitchAssistantResult::kSuccess;
  }

  DVLOG(1) << "[glic] SwitchAssistant target=" << target
           << " result=" << response->result;
  GlicMetricReporter(&profile).RecordEnumeration(
      base::StrCat({"SwitchAssistant.Result.",
                    GetAssistantTargetHistogramSuffix(target)}),
      response->result);

  const bool should_relaunch =
      (response->result == mojom::SwitchAssistantResult::kSuccess);
  std::move(callback).Run(std::move(response));

  if (should_relaunch) {
    RestartBrowserNow();
  }
}

void MaybeRelaunchForProviderChange(Profile* profile) {
  if (GlicEnabling::GetPendingProviderChange(profile) ==
      GlicProviderChange::kNone) {
    return;
  }

  base::RecordAction(
      base::UserMetricsAction("Glic.ProviderChange.RelaunchClicked"));
  // Only `profile` is marked: other profiles that pick up a provider change
  // from this relaunch didn't see the prompt, so they still get the notice.
  RecordUserInitiatedProviderSwitch(*profile);
  RestartBrowserNow();
}

}  // namespace glic
