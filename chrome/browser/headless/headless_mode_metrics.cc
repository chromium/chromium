// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/headless_mode_metrics.h"

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "components/headless/command_handler/headless_command_switches.h"
#include "content/public/common/content_switches.h"

namespace headless {

namespace {

// This enum is used in UMA histograms, so do not remove or renumber entries.
// If you're adding entries to this enum, update the ChromeHeadlessAction enum
// in tools/metrics/histograms/enums.xml
enum class HeadlessChromeAction {
  kNone = 0,
  kRemoteDebuggingPipe = 1,
  kRemoteDebuggingPort = 2,
  kDumpDom = 3,
  kScreenshot = 4,
  kPrintToPDF = 5,
  kMaxValue = kPrintToPDF
};

HeadlessChromeAction GetHeadlessChromeAction() {
  static struct {
    const char* command_line_switch;
    HeadlessChromeAction action;
  } kSwitchActions[] = {
      {::switches::kRemoteDebuggingPipe,
       HeadlessChromeAction::kRemoteDebuggingPipe},
      {::switches::kRemoteDebuggingPort,
       HeadlessChromeAction::kRemoteDebuggingPort},
      {switches::kDumpDom, HeadlessChromeAction::kDumpDom},
      {switches::kScreenshot, HeadlessChromeAction::kScreenshot},
      {switches::kPrintToPDF, HeadlessChromeAction::kPrintToPDF},
  };

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  for (const auto& switch_action : kSwitchActions) {
    if (command_line.HasSwitch(switch_action.command_line_switch)) {
      return switch_action.action;
    }
  }

  return HeadlessChromeAction::kNone;
}

}  // namespace

void ReportHeadlessActionMetrics() {
  DCHECK(IsHeadlessMode());

  HeadlessChromeAction action = GetHeadlessChromeAction();
  base::UmaHistogramEnumeration("Chrome.Headless.Action", action);
}

}  // namespace headless
