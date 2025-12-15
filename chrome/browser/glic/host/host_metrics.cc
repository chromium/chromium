// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/host_metrics.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace glic {

HostMetrics::HostMetrics(Host* host) : host_(host) {}

HostMetrics::~HostMetrics() = default;

void HostMetrics::StartRecording() {
  if (base::FeatureList::IsEnabled(
          features::kGlicRecordMemoryFootprintMetrics)) {
    memory_metrics_timer_.Start(FROM_HERE, base::Minutes(30), this,
                                &HostMetrics::RecordMemoryMetrics);
  }
}

void HostMetrics::Shutdown() {
  memory_metrics_timer_.Stop();
  if (base::FeatureList::IsEnabled(
          features::kGlicRecordMemoryFootprintMetrics)) {
    if (max_webui_memory_ > 0) {
      base::UmaHistogramMemoryLargeMB(
          "Glic.Instance.WebUI.MaxPrivateMemoryFootprint",
          max_webui_memory_ / 1024 / 1024);
    }
    if (max_web_client_memory_ > 0) {
      base::UmaHistogramMemoryLargeMB(
          "Glic.Instance.WebClient.MaxPrivateMemoryFootprint",
          max_web_client_memory_ / 1024 / 1024);
    }
  }
}

void HostMetrics::RecordMemoryMetrics() {
  // Record WebUI memory
  if (content::WebContents* contents = host_->webui_contents()) {
    if (content::RenderProcessHost* process =
            contents->GetPrimaryMainFrame()->GetProcess()) {
      OnPrivateMemoryFootprint(/*is_webui=*/true,
                               process->GetPrivateMemoryFootprint());
    }
  }

  // Record WebClient memory
  if (content::WebContents* contents = host_->web_client_contents()) {
    if (content::RenderProcessHost* process =
            contents->GetPrimaryMainFrame()->GetProcess()) {
      OnPrivateMemoryFootprint(/*is_webui=*/false,
                               process->GetPrivateMemoryFootprint());
    }
  }
}

void HostMetrics::OnPrivateMemoryFootprint(bool is_webui, uint64_t bytes) {
  if (is_webui) {
    int memory_mb = bytes / 1024 / 1024;
    base::UmaHistogramMemoryLargeMB(
        "Glic.Instance.WebUI.PrivateMemoryFootprint", memory_mb);
    if (bytes > max_webui_memory_) {
      max_webui_memory_ = bytes;
    }
  } else {
    int memory_mb = bytes / 1024 / 1024;
    base::UmaHistogramMemoryLargeMB(
        "Glic.Instance.WebClient.PrivateMemoryFootprint", memory_mb);
    if (bytes > max_web_client_memory_) {
      max_web_client_memory_ = bytes;
    }
  }
}

}  // namespace glic
