// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/printing_metrics_ash.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/ash/crosapi/print_job_info_idl_conversions.h"
#include "chrome/browser/ash/printing/history/print_job_history_service.h"
#include "chrome/browser/ash/printing/history/print_job_history_service_factory.h"
#include "chrome/browser/ash/printing/history/print_job_info.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace crosapi {

PrintingMetricsForProfileAsh::PrintingMetricsForProfileAsh(
    Profile* profile,
    mojo::PendingRemote<crosapi::mojom::PrintJobObserverForProfile> observer)
    : profile_(profile), observer_(std::move(observer)) {
  auto* history_service =
      ash::PrintJobHistoryServiceFactory::GetForBrowserContext(profile_);
  // The print job history service is not available on the lock screen.
  if (history_service) {
    print_job_history_service_observation_.Observe(history_service);
  }
}

PrintingMetricsForProfileAsh::~PrintingMetricsForProfileAsh() = default;

void PrintingMetricsForProfileAsh::DeprecatedGetPrintJobs(
    DeprecatedGetPrintJobsCallback callback) {
  NOTIMPLEMENTED();
}

void PrintingMetricsForProfileAsh::GetPrintJobs(GetPrintJobsCallback callback) {
  ash::PrintJobHistoryServiceFactory::GetForBrowserContext(profile_)
      ->GetPrintJobs(
          base::BindOnce(&PrintingMetricsForProfileAsh::OnPrintJobsRetrieved,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
}

void PrintingMetricsForProfileAsh::OnPrintJobFinished(
    const ash::printing::proto::PrintJobInfo& print_job_info) {
  auto dict_value =
      extensions::PrintJobInfoProtoToIdl(print_job_info).ToValue();
  observer_->OnPrintJobFinished(base::Value(std::move(dict_value)));
}

void PrintingMetricsForProfileAsh::OnShutdown() {
  // ash::PrintJobHistoryService might go out of scope earlier than the ash
  // service since we don't declare any factory dependencies here. Therefore
  // it's safer to reset the observer at this point.
  print_job_history_service_observation_.Reset();
  profile_ = nullptr;
}

void PrintingMetricsForProfileAsh::OnPrintJobsRetrieved(
    GetPrintJobsCallback callback,
    bool success,
    std::vector<ash::printing::proto::PrintJobInfo> print_job_infos) {
  if (!success) {
    std::move(callback).Run(/*print_jobs=*/{});
    return;
  }

  base::Value::List print_job_info_values;
  for (const auto& print_job_info : print_job_infos) {
    auto dict_value =
        extensions::PrintJobInfoProtoToIdl(print_job_info).ToValue();
    print_job_info_values.Append(std::move(dict_value));
  }

  std::move(callback).Run(std::move(print_job_info_values));
}

PrintingMetricsAsh::PrintingMetricsAsh() = default;
PrintingMetricsAsh::~PrintingMetricsAsh() = default;

void PrintingMetricsAsh::RegisterForMainProfile(
    mojo::PendingReceiver<crosapi::mojom::PrintingMetricsForProfile> receiver,
    mojo::PendingRemote<crosapi::mojom::PrintJobObserverForProfile> observer) {
  RegisterForProfile(ProfileManager::GetPrimaryUserProfile(),
                     std::move(receiver), std::move(observer));
}

void PrintingMetricsAsh::RegisterForProfile(
    Profile* profile,
    mojo::PendingReceiver<crosapi::mojom::PrintingMetricsForProfile> receiver,
    mojo::PendingRemote<crosapi::mojom::PrintJobObserverForProfile> observer) {
  DCHECK(profile);
  auto service = std::make_unique<PrintingMetricsForProfileAsh>(
      profile, std::move(observer));
  receivers_.Add(std::move(service), std::move(receiver));
}

void PrintingMetricsAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::PrintingMetrics> receiver) {
  crosapi_receivers_.Add(this, std::move(receiver));
}

}  // namespace crosapi
