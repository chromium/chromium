// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/assistant_service_metrics_provider.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ash/assistant/assistant_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"
#include "components/prefs/pref_service.h"

AssistantServiceMetricsProvider::AssistantServiceMetricsProvider() = default;
AssistantServiceMetricsProvider::~AssistantServiceMetricsProvider() = default;

void AssistantServiceMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto_unused) {
  if (assistant::IsAssistantAllowedForProfile(
          ProfileManager::GetActiveUserProfile()) !=
      ash::assistant::AssistantAllowedState::ALLOWED) {
    return;
  }

  UMA_HISTOGRAM_BOOLEAN(
      "Assistant.ServiceEnabledUserCount",
      ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
          ash::assistant::prefs::kAssistantEnabled));

  UMA_HISTOGRAM_BOOLEAN(
      "Assistant.ContextEnabledUserCount",
      ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
          ash::assistant::prefs::kAssistantContextEnabled));
}
