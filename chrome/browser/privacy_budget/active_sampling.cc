// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/active_sampling.h"

#include "content/public/common/user_agent.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_sample_collector.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"

namespace {

void ActivelySampleUserAgentModel() {
  ukm::SourceId ukm_source_id = ukm::NoURLSourceId();
  content::BuildModelInfo();
  auto identifiable_surface = blink::IdentifiableSurface::FromTypeAndToken(
      blink::IdentifiableSurface::Type::kNavigatorUAData_GetHighEntropyValues,
      blink::IdentifiableToken("model"));
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder) {
    return;
  }
  blink::IdentifiabilityMetricBuilder(ukm_source_id)
      .Add(identifiable_surface,
           blink::IdentifiableToken(content::BuildModelInfo()))
      .Record(ukm_recorder);
  blink::IdentifiabilitySampleCollector::Get()->FlushSource(ukm_recorder,
                                                            ukm_source_id);
}

}  // namespace

void ActivelySampleIdentifiableSurfaces() {
  if (!blink::IdentifiabilityStudySettings::Get()->ShouldActivelySample())
    return;
  ActivelySampleUserAgentModel();
}
