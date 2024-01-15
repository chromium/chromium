// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing_metrics/printing_metrics_service.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/printing_metrics.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/printing_metrics_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

#if BUILDFLAG(IS_CHROMEOS_LACROS)

// Only main profile should be allowed to access the API.
bool IsContextForMainProfile(content::BrowserContext* context) {
  return Profile::FromBrowserContext(context)->IsMainProfile();
}

crosapi::mojom::PrintingMetrics* GetPrintingMetrics() {
  auto* service = chromeos::LacrosService::Get();
  if (!service->IsRegistered<crosapi::mojom::PrintingMetrics>() ||
      !service->IsAvailable<crosapi::mojom::PrintingMetrics>()) {
    LOG(ERROR) << "chrome.printingMetrics is not available in Lacros";
    return nullptr;
  }
  return service->GetRemote<crosapi::mojom::PrintingMetrics>().get();
}

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

namespace extensions {

// static
BrowserContextKeyedAPIFactory<PrintingMetricsService>*
PrintingMetricsService::GetFactoryInstance() {
  static base::NoDestructor<
      BrowserContextKeyedAPIFactory<PrintingMetricsService>>
      instance;
  return instance.get();
}

// static
PrintingMetricsService* PrintingMetricsService::Get(
    content::BrowserContext* context) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!IsContextForMainProfile(context) || !GetPrintingMetrics()) {
    return nullptr;
  }
#endif  // BUIDLFLAG(IS_CHROMEOS_LACROS)
  return BrowserContextKeyedAPIFactory<PrintingMetricsService>::Get(context);
}

PrintingMetricsService::PrintingMetricsService(content::BrowserContext* context)
    : context_(context) {
  EventRouter::Get(context_)->RegisterObserver(
      this, api::printing_metrics::OnPrintJobFinished::kEventName);
}

PrintingMetricsService::~PrintingMetricsService() = default;

void PrintingMetricsService::Shutdown() {
  EventRouter::Get(context_)->UnregisterObserver(this);
}

void PrintingMetricsService::OnListenerAdded(const EventListenerInfo&) {
  // Ensures that the two-way crosapi connection is initialized for the profile,
  // so that incoming onPrintJobFinished events can be dispatched to the
  // extension.
  EnsureInit();
}

void PrintingMetricsService::GetPrintJobs(
    crosapi::mojom::PrintingMetricsForProfile::GetPrintJobsCallback callback) {
  EnsureInit();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // We might need to call the older version if Ash version is not up-to-date.
  if (chromeos::LacrosService::Get()
          ->GetInterfaceVersion<crosapi::mojom::PrintingMetricsForProfile>() <
      static_cast<int>(
          crosapi::mojom::PrintingMetricsForProfile::kGetPrintJobsMinVersion)) {
    remote_->DeprecatedGetPrintJobs(
        base::BindOnce([](std::vector<base::Value> print_jobs) {
          base::Value::List jobs;
          for (auto& print_job : print_jobs) {
            jobs.Append(std::move(print_job));
          }
          return jobs;
        }).Then(std::move(callback)));
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  remote_->GetPrintJobs(std::move(callback));
}

void PrintingMetricsService::OnPrintJobFinished(base::Value print_job) {
  std::optional<api::printing_metrics::PrintJobInfo> print_job_info =
      api::printing_metrics::PrintJobInfo::FromValue(std::move(print_job));
  DCHECK(print_job_info.has_value());

  auto event = std::make_unique<Event>(
      events::PRINTING_METRICS_ON_PRINT_JOB_FINISHED,
      api::printing_metrics::OnPrintJobFinished::kEventName,
      api::printing_metrics::OnPrintJobFinished::Create(
          std::move(print_job_info).value()));

  EventRouter::Get(context_)->BroadcastEvent(std::move(event));
}

void PrintingMetricsService::EnsureInit() {
  if (initialized_) {
    return;
  }
  initialized_ = true;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  crosapi::PrintingMetricsAsh* printing_metrics_ash =
      crosapi::CrosapiManager::Get()->crosapi_ash()->printing_metrics_ash();

  printing_metrics_ash->RegisterForProfile(
      Profile::FromBrowserContext(context_),
      remote_.BindNewPipeAndPassReceiver(),
      receiver_.BindNewPipeAndPassRemote());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  GetPrintingMetrics()->RegisterForMainProfile(
      remote_.BindNewPipeAndPassReceiver(),
      receiver_.BindNewPipeAndPassRemote());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

}  // namespace extensions
