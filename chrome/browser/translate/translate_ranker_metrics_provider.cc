// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_ranker_metrics_provider.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/translate/translate_ranker_factory.h"
#include "components/translate/core/browser/translate_ranker.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/translate_event.pb.h"

namespace translate {

TranslateRankerMetricsProvider::TranslateRankerMetricsProvider()
    : logging_enabled_(false) {
  g_browser_process->profile_manager()->AddObserver(this);
}

TranslateRankerMetricsProvider::~TranslateRankerMetricsProvider() {
  g_browser_process->profile_manager()->RemoveObserver(this);
}

void TranslateRankerMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  std::vector<Profile*> loaded_profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (Profile* profile : loaded_profiles) {
    TranslateRanker* ranker =
        TranslateRankerFactory::GetForBrowserContext(profile);
    if (!ranker)
      continue;

    UpdateLoggingState();
    std::vector<metrics::TranslateEventProto> translate_events;
    ranker->FlushTranslateEvents(&translate_events);

    for (auto& event : translate_events) {
      uma_proto->add_translate_event()->Swap(&event);
    }
  }
}

void TranslateRankerMetricsProvider::UpdateLoggingState() {
  std::vector<Profile*> loaded_profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (Profile* profile : loaded_profiles) {
    TranslateRanker* ranker =
        TranslateRankerFactory::GetForBrowserContext(profile);
    if (ranker)
      ranker->EnableLogging(logging_enabled_);
  }
}

void TranslateRankerMetricsProvider::OnRecordingEnabled() {
  logging_enabled_ = true;
  UpdateLoggingState();
}

void TranslateRankerMetricsProvider::OnRecordingDisabled() {
  logging_enabled_ = false;
  UpdateLoggingState();
}

void TranslateRankerMetricsProvider::OnProfileAdded(Profile* profile) {
  UpdateLoggingState();
}

}  // namespace translate
