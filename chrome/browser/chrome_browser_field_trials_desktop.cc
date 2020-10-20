// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials_desktop.h"

#include <map>
#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/metrics/persistent_system_profile.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/common/content_switches.h"

namespace chrome {

namespace {

void SetupStunProbeTrial() {
  std::map<std::string, std::string> params;
  if (!variations::GetVariationParams("StunProbeTrial2", &params))
    return;

  // The parameter, used by StartStunFieldTrial, should have the following
  // format: "request_per_ip/interval/sharedsocket/batch_size/total_batches/
  // server1:port/server2:port/server3:port/"
  std::string cmd_param = params["request_per_ip"] + "/" + params["interval"] +
                          "/" + params["sharedsocket"] + "/" +
                          params["batch_size"] + "/" + params["total_batches"] +
                          "/" + params["server1"] + "/" + params["server2"] +
                          "/" + params["server3"] + "/" + params["server4"] +
                          "/" + params["server5"] + "/" + params["server6"];

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kWebRtcStunProbeTrialParameter, cmd_param);
}

}  // namespace

void SetupDesktopFieldTrials() {
  SetupStunProbeTrial();
}

}  // namespace chrome
