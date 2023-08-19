// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/federated_metrics_manager.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chromeos/ash/services/federated/public/cpp/federated_example_util.h"
#include "chromeos/ash/services/federated/public/mojom/example.mojom.h"
#include "chromeos/ash/services/federated/public/mojom/federated_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace app_list::federated {

namespace {

using ash::federated::CreateStringList;
using chromeos::federated::mojom::Example;
using chromeos::federated::mojom::ExamplePtr;
using chromeos::federated::mojom::Features;

constexpr char kClientName[] = "launcher_query_analytics_v1";

std::string SearchSessionConclusionToString(
    ash::SearchSessionConclusion conclusion) {
  switch (conclusion) {
    case ash::SearchSessionConclusion::kQuit:
      return "quit";
    case ash::SearchSessionConclusion::kLaunch:
      return "launch";
    case ash::SearchSessionConclusion::kAnswerCardSeen:
      return "answer_card";
    default:
      NOTREACHED();
  }
}

void LogSearchSessionConclusion(ash::SearchSessionConclusion conclusion) {
  base::UmaHistogramEnumeration(kHistogramSearchSessionConclusion, conclusion);
}

void LogInitStatus(FederatedMetricsManager::InitStatus status) {
  base::UmaHistogramEnumeration(kHistogramInitStatus, status);
}

void LogReportStatus(FederatedMetricsManager::ReportStatus status) {
  base::UmaHistogramEnumeration(kHistogramReportStatus, status);
}

ExamplePtr CreateExamplePtr(const std::string& query,
                            const std::string& user_action) {
  ExamplePtr example = Example::New();
  example->features = Features::New();
  auto& feature_map = example->features->feature;
  // TODO(b/262611120): To be decided: Conversion to lowercase, white space
  // stripping, truncation, etc. of query.
  feature_map["query"] = CreateStringList({query});
  feature_map["user_action"] = CreateStringList({user_action});
  return example;
}

bool AreFeatureFlagsEnabled() {
  return ash::features::IsFederatedServiceEnabled() &&
         search_features::IsLauncherQueryFederatedAnalyticsPHHEnabled();
}

}  // namespace

FederatedMetricsManager::FederatedMetricsManager(
    ash::AppListNotifier* notifier,
    ash::federated::FederatedServiceController* controller)
    : controller_(controller) {
  if (!AreFeatureFlagsEnabled()) {
    // Don't log InitStatus metrics if the feature is disabled.
    return;
  }
  if (!notifier) {
    LogInitStatus(InitStatus::kMissingNotifier);
    return;
  }

  if (!controller_) {
    LogInitStatus(InitStatus::kMissingController);
    return;
  }

  // Observe notifier only after all init checks have suceeded.
  observation_.Observe(notifier);
  LogInitStatus(InitStatus::kOk);
}

FederatedMetricsManager::~FederatedMetricsManager() = default;

void FederatedMetricsManager::OnSearchSessionStarted() {
  if (!IsLoggingEnabled()) {
    return;
  }
  session_active_ = true;
}

void FederatedMetricsManager::OnSearchSessionEnded(
    const std::u16string& query) {
  if (!IsLoggingEnabled() || query.empty() || !session_active_) {
    return;
  }
  // UMA logging:
  LogSearchSessionConclusion(session_result_);
  // Federated logging:
  LogExample(base::UTF16ToUTF8(query));

  session_result_ = ash::SearchSessionConclusion::kQuit;
  session_active_ = false;
}

void FederatedMetricsManager::OnSeen(Location location,
                                     const std::vector<Result>& results,
                                     const std::u16string& query) {
  if (!IsLoggingEnabled() || query.empty()) {
    return;
  }
  if (location == Location::kAnswerCard) {
    DCHECK(session_active_);
    session_result_ = ash::SearchSessionConclusion::kAnswerCardSeen;
  }
}

void FederatedMetricsManager::OnLaunch(Location location,
                                       const Result& launched,
                                       const std::vector<Result>& shown,
                                       const std::u16string& query) {
  if (!IsLoggingEnabled() || query.empty()) {
    return;
  }
  if (location == Location::kList || location == Location::kAnswerCard) {
    DCHECK(session_active_);
    session_result_ = ash::SearchSessionConclusion::kLaunch;
  }
}

void FederatedMetricsManager::OnDefaultSearchIsGoogleSet(bool is_google) {
  is_default_search_engine_google_ = is_google;
}

bool FederatedMetricsManager::IsFederatedServiceAvailable() {
  return controller_ && controller_->IsServiceAvailable();
}

bool FederatedMetricsManager::IsLoggingEnabled() {
  CHECK(is_default_search_engine_google_.has_value());
  return ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled() &&
         AreFeatureFlagsEnabled() && is_default_search_engine_google_.value();
}

void FederatedMetricsManager::TryToBindFederatedServiceIfNecessary() {
  if (federated_service_.is_bound()) {
    return;
  }

  if (IsFederatedServiceAvailable()) {
    ash::federated::ServiceConnection::GetInstance()->BindReceiver(
        federated_service_.BindNewPipeAndPassReceiver());
  }
}

void FederatedMetricsManager::LogExample(const std::string& query) {
  if (!IsLoggingEnabled()) {
    return;
  }

  TryToBindFederatedServiceIfNecessary();

  if (!IsFederatedServiceAvailable()) {
    LogReportStatus(ReportStatus::kFederatedServiceNotAvailable);
  } else if (!federated_service_.is_connected()) {
    LogReportStatus(ReportStatus::kFederatedServiceNotConnected);
  } else {
    // Federated service available and connected.
    ExamplePtr example = CreateExamplePtr(
        query, SearchSessionConclusionToString(session_result_));
    federated_service_->ReportExample(kClientName, std::move(example));
    LogReportStatus(ReportStatus::kOk);
  }
}

}  // namespace app_list::federated
