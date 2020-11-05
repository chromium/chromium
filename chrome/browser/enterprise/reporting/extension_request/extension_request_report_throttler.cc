// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/extension_request/extension_request_report_throttler.h"

#include "base/no_destructor.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"

namespace enterprise_reporting {

// static
ExtensionRequestReportThrottler* ExtensionRequestReportThrottler::Get() {
  static base::NoDestructor<ExtensionRequestReportThrottler> throttler;
  return throttler.get();
}

ExtensionRequestReportThrottler::ExtensionRequestReportThrottler() = default;
ExtensionRequestReportThrottler::~ExtensionRequestReportThrottler() = default;

void ExtensionRequestReportThrottler::Enable(
    base::TimeDelta throttle_time,
    base::RepeatingClosure report_trigger) {
  DCHECK(report_trigger);
  DCHECK(!report_trigger_);
  report_trigger_ = report_trigger;
  throttle_timer_ = std::make_unique<base::RetainingOneShotTimer>(
      FROM_HERE, throttle_time,
      base::BindRepeating(&ExtensionRequestReportThrottler::MaybeUpload,
                          base::Unretained(this)));
  ongoing_upload_ = false;
  ResetProfiles();
}

void ExtensionRequestReportThrottler::Disable() {
  report_trigger_.Reset();
  throttle_timer_.reset();
}

bool ExtensionRequestReportThrottler::IsEnabled() const {
  return !report_trigger_.is_null();
}

void ExtensionRequestReportThrottler::AddProfile(
    const base::FilePath& profile_path) {
  DCHECK(IsEnabled());
  profiles_.emplace(profile_path);
  MaybeUpload();
}

const base::flat_set<base::FilePath>&
ExtensionRequestReportThrottler::GetProfiles() const {
  return profiles_;
}

void ExtensionRequestReportThrottler::ResetProfiles() {
  profiles_.clear();
}

void ExtensionRequestReportThrottler::OnExtensionRequestUploaded() {
  DCHECK(IsEnabled());
  ongoing_upload_ = false;
  MaybeUpload();
}

bool ExtensionRequestReportThrottler::ShouldUpload() {
  return !throttle_timer_->IsRunning() && !ongoing_upload_ &&
         !profiles_.empty();
}

void ExtensionRequestReportThrottler::MaybeUpload() {
  if (!ShouldUpload())
    return;
  ongoing_upload_ = true;
  throttle_timer_->Reset();
  report_trigger_.Run();
}

}  // namespace enterprise_reporting
