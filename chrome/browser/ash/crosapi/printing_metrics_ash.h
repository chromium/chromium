// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_PRINTING_METRICS_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_PRINTING_METRICS_ASH_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/printing/history/print_job_history_service.h"
#include "chrome/browser/ash/printing/history/print_job_info.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/crosapi/mojom/printing_metrics.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "printing/buildflags/buildflags.h"

#if !BUILDFLAG(USE_CUPS)
#error PrintingMetricsAsh must be used with the USE_CUPS flag.
#endif

namespace crosapi {
// Ash implementation of crosapi::mojom::PrintingMetricsForProfile.
// This class communicates with ash::PrintJobHistory service for the given
// profile -- queries print jobs and listens to finished events.
class PrintingMetricsForProfileAsh
    : public crosapi::mojom::PrintingMetricsForProfile,
      public ash::PrintJobHistoryService::Observer {
 public:
  PrintingMetricsForProfileAsh(
      Profile* profile,
      mojo::PendingRemote<crosapi::mojom::PrintJobObserverForProfile> observer);
  ~PrintingMetricsForProfileAsh() override;

  // crosapi::mojom::PrintingMetricsForProfile:
  void DeprecatedGetPrintJobs(DeprecatedGetPrintJobsCallback) override;
  void GetPrintJobs(GetPrintJobsCallback) override;

  // ash::PrintJobHistoryService::Observer:
  void OnPrintJobFinished(
      const ash::printing::proto::PrintJobInfo& print_job_info) override;
  void OnShutdown() override;

 private:
  void OnPrintJobsRetrieved(GetPrintJobsCallback,
                            bool success,
                            std::vector<ash::printing::proto::PrintJobInfo>);

  raw_ptr<Profile> profile_ = nullptr;

  base::ScopedObservation<ash::PrintJobHistoryService,
                          ash::PrintJobHistoryService::Observer>
      print_job_history_service_observation_{this};

  mojo::Remote<crosapi::mojom::PrintJobObserverForProfile> observer_;

  base::WeakPtrFactory<PrintingMetricsForProfileAsh> weak_factory_{this};
};

// Ash implementation for crosapi::mojom::PrintingMetrics.
// This class creates bridges between ash and lacros -- the actual
// processing happens in PrintingMetricsForProfileAsh.
class PrintingMetricsAsh : public crosapi::mojom::PrintingMetrics {
 public:
  PrintingMetricsAsh();
  ~PrintingMetricsAsh() override;

  // crosapi::mojom::PrintingMetrics:
  void RegisterForMainProfile(
      mojo::PendingReceiver<crosapi::mojom::PrintingMetricsForProfile> receiver,
      mojo::PendingRemote<crosapi::mojom::PrintJobObserverForProfile> observer)
      override;

  void RegisterForProfile(
      Profile* profile,
      mojo::PendingReceiver<crosapi::mojom::PrintingMetricsForProfile> receiver,
      mojo::PendingRemote<crosapi::mojom::PrintJobObserverForProfile> observer);

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::PrintingMetrics> receiver);

 private:
  mojo::UniqueReceiverSet<crosapi::mojom::PrintingMetricsForProfile> receivers_;

  mojo::ReceiverSet<crosapi::mojom::PrintingMetrics> crosapi_receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_PRINTING_METRICS_ASH_H_
