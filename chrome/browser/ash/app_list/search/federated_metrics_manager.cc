// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/federated_metrics_manager.h"

#include "ash/shell.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
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

// Prefixes are short, for bandwidth conservation.
constexpr char kExamplePrefixOnAbandon[] = "A_";
constexpr char kExamplePrefixOnLaunch[] = "L_";

bool IsLoggingEnabled() {
  // TODO(b/262611120): Also check user metrics opt-in/out, any other relevant
  // federated flags, etc.
  return search_features::IsLauncherQueryFederatedAnalyticsPHHEnabled();
}

void LogAction(FederatedMetricsManager::Action action) {
  base::UmaHistogramEnumeration(kHistogramAction, action);
}

void LogInitStatus(FederatedMetricsManager::InitStatus status) {
  base::UmaHistogramEnumeration(kHistogramInitStatus, status);
}

void LogReportStatus(FederatedMetricsManager::ReportStatus status) {
  base::UmaHistogramEnumeration(kHistogramReportStatus, status);
}

ExamplePtr CreateExamplePtr(const std::string& example_str) {
  ExamplePtr example = Example::New();
  example->features = Features::New();
  auto& feature_map = example->features->feature;
  feature_map["query"] = CreateStringList({example_str});
  return example;
}

std::string CreateExampleString(const std::string& prefix,
                                const std::u16string& query) {
  // TODO(b/262611120): To be decided: Conversion to lowercase, white space
  // stripping, truncation, etc.
  return base::StrCat({prefix, base::UTF16ToUTF8(query)});
}

}  // namespace

FederatedMetricsManager::FederatedMetricsManager(
    ash::AppListNotifier* notifier,
    ash::federated::FederatedServiceController* controller)
    : controller_(controller) {
  if (!IsLoggingEnabled()) {
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

  TryToBindFederatedServiceIfNecessary();
  if (!federated_service_.is_bound() || !federated_service_.is_connected()) {
    LogInitStatus(InitStatus::kFederatedConnectionFailedToEstablish);
    return;
  }

  // Observe notifier only after all init checks have suceeded.
  observation_.Observe(notifier);
  LogInitStatus(InitStatus::kOk);
}

FederatedMetricsManager::~FederatedMetricsManager() = default;

void FederatedMetricsManager::OnAbandon(Location location,
                                        const std::vector<Result>& results,
                                        const std::u16string& query) {
  if (!IsLoggingEnabled() || query.empty()) {
    return;
  }
  LogAction(Action::kAbandon);
  LogExample(CreateExampleString(kExamplePrefixOnAbandon, query));
}

void FederatedMetricsManager::OnLaunch(Location location,
                                       const Result& launched,
                                       const std::vector<Result>& shown,
                                       const std::u16string& query) {
  if (!IsLoggingEnabled() || query.empty()) {
    return;
  }
  LogAction(Action::kLaunch);
  LogExample(CreateExampleString(kExamplePrefixOnLaunch, query));
}

bool FederatedMetricsManager::IsFederatedServiceAvailable() {
  return controller_ && controller_->IsServiceAvailable();
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

void FederatedMetricsManager::LogExample(const std::string& example_str) {
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
    ExamplePtr example = CreateExamplePtr(example_str);
    federated_service_->ReportExample(kClientName, std::move(example));
    LogReportStatus(ReportStatus::kOk);
  }
}

}  // namespace app_list::federated
