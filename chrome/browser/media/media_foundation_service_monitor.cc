// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_foundation_service_monitor.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/power_monitor/power_monitor.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cdm_registry.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/media_foundation_service.mojom.h"
#include "ui/display/screen.h"

namespace {

constexpr int kMaxNumberOfSamples = 20;
constexpr auto kGracePeriod = base::Seconds(2);
constexpr double kMaxAverageFailureScore = 0.1;  // Two failures out of 20.

constexpr int kSignificantPlayback = 0;  // This must always be zero.
constexpr int kPlaybackOrCdmError = 1;
constexpr int kCrash = 1;

}  // namespace

// static
MediaFoundationServiceMonitor* MediaFoundationServiceMonitor::GetInstance() {
  static auto* monitor = new MediaFoundationServiceMonitor();
  return monitor;
}

MediaFoundationServiceMonitor::MediaFoundationServiceMonitor()
    : samples_(kMaxNumberOfSamples) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Initialize samples with success cases so the average score won't be
  // dominated by one error.
  for (int i = 0; i < kMaxNumberOfSamples; ++i)
    AddSample(kSignificantPlayback);

  content::ServiceProcessHost::AddObserver(this);
  base::PowerMonitor::AddPowerSuspendObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
}

MediaFoundationServiceMonitor::~MediaFoundationServiceMonitor() = default;

void MediaFoundationServiceMonitor::OnServiceProcessCrashed(
    const content::ServiceProcessInfo& info) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Only interested in MediaFoundationService process.
  if (!info.IsService<media::mojom::MediaFoundationServiceBroker>())
    return;

  // Not checking `last_power_or_display_change_time_`; crashes are always bad.
  DVLOG(1) << __func__ << ": MediaFoundationService process crashed!";
  AddSample(kCrash);
}

void MediaFoundationServiceMonitor::OnSuspend() {
  OnPowerOrDisplayChange();
}

void MediaFoundationServiceMonitor::OnResume() {
  OnPowerOrDisplayChange();
}

void MediaFoundationServiceMonitor::OnDisplayAdded(
    const display::Display& /*new_display*/) {
  OnPowerOrDisplayChange();
}
void MediaFoundationServiceMonitor::OnDidRemoveDisplays() {
  OnPowerOrDisplayChange();
}
void MediaFoundationServiceMonitor::OnDisplayMetricsChanged(
    const display::Display& /*display*/,
    uint32_t /*changed_metrics*/) {
  OnPowerOrDisplayChange();
}

void MediaFoundationServiceMonitor::OnSignificantPlayback() {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  AddSample(kSignificantPlayback);
}

void MediaFoundationServiceMonitor::OnPlaybackOrCdmError() {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (base::Time::Now() - last_power_or_display_change_time_ < kGracePeriod) {
    DVLOG(1) << "Playback or CDM error ignored since it happened right after "
                "a power or display change.";
    return;
  }

  AddSample(kPlaybackOrCdmError);
}

void MediaFoundationServiceMonitor::OnPowerOrDisplayChange() {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  last_power_or_display_change_time_ = base::Time::Now();
}

void MediaFoundationServiceMonitor::AddSample(int failure_score) {
  samples_.AddSample(failure_score);

  if (samples_.GetUnroundedAverage() >= kMaxAverageFailureScore &&
      base::FeatureList::IsEnabled(media::kHardwareSecureDecryptionFallback)) {
    content::CdmRegistry::GetInstance()->DisableHardwareSecureCdms();
  }
}
