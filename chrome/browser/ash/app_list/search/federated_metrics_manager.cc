// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/federated_metrics_manager.h"

#include "ash/shell.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/ash/app_list/search/search_metrics_util.h"
#include "chromeos/ash/services/federated/public/cpp/federated_example_util.h"
#include "chromeos/ash/services/federated/public/mojom/example.mojom.h"
#include "chromeos/ash/services/federated/public/mojom/federated_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace app_list {

namespace {

using ash::federated::CreateStringList;
using chromeos::federated::mojom::Example;
using chromeos::federated::mojom::ExamplePtr;
using chromeos::federated::mojom::Features;

constexpr char kClientName[] = "launcher_query_analytics_v1";
// TODO(b/262611120): Consider shortening these, for potential bandwidth
// conservation.
constexpr char kExamplePrefixOnAbandon[] = "ABANDON_";
constexpr char kExamplePrefixOnLaunch[] = "LAUNCH_";

bool IsLoggingEnabled() {
  // TODO(b/262611120): Also check user metrics opt-in/out, any other relevant
  // federated flags, etc.
  return search_features::IsLauncherQueryFederatedAnalyticsPHHEnabled();
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
    // TODO(b/262611120): Metrics (missing notifier).
    return;
  }

  if (!controller_) {
    // TODO(b/262611120): Metrics (missing controller).
  }

  observation_.Observe(notifier);
  TryToBindFederatedServiceIfNecessary();

  if (!federated_service_.is_bound() || !federated_service_.is_connected()) {
    // TODO(b/262611120): Metrics (failed connection).
  }
}

FederatedMetricsManager::~FederatedMetricsManager() = default;

void FederatedMetricsManager::OnAbandon(Location location,
                                        const std::vector<Result>& results,
                                        const std::u16string& query) {
  if (!IsLoggingEnabled()) {
    return;
  }
  // TODO(b/262611120): Metrics (count abandons).
  LogExample(CreateExampleString(kExamplePrefixOnAbandon, query));
}

void FederatedMetricsManager::OnLaunch(Location location,
                                       const Result& launched,
                                       const std::vector<Result>& shown,
                                       const std::u16string& query) {
  if (!IsLoggingEnabled()) {
    return;
  }
  // TODO(b/262611120): Metrics (count launches).
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
  if (IsFederatedServiceAvailable() && federated_service_.is_connected()) {
    ExamplePtr example = CreateExamplePtr(example_str);
    federated_service_->ReportExample(kClientName, std::move(example));
  } else {
    // TODO(b/262611120): Error handling.
  }
}

}  // namespace app_list
