// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/domain_reliability/service_factory.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/common/chrome_switches.h"
#include "components/domain_reliability/domain_reliability_prefs.h"
#include "components/prefs/pref_service.h"

namespace domain_reliability {

namespace {

// If Domain Reliability is enabled in the absence of a flag or field trial.
const bool kDefaultEnabled = true;

// The name and value of the field trial to turn Domain Reliability on.
const char kFieldTrialName[] = "DomRel-Enable";
const char kFieldTrialValueEnable[] = "enable";

bool IsDomainReliabilityAllowed() {
  return g_browser_process->local_state()->GetBoolean(
      prefs::kDomainReliabilityAllowedByPolicy);
}

}  // namespace

const char kUploadReporterString[] = "chrome";

bool ShouldCreateService() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableDomainReliability))
    return false;
  if (command_line->HasSwitch(switches::kEnableDomainReliability))
    return true;
  if (!IsDomainReliabilityAllowed()) {
    return false;
  }
  if (!ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled())
    return false;
  if (base::FieldTrialList::TrialExists(kFieldTrialName)) {
    std::string value = base::FieldTrialList::FindFullName(kFieldTrialName);
    return (value == kFieldTrialValueEnable);
  }
  return kDefaultEnabled;
}

}  // namespace domain_reliability
