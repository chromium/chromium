// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/guest_tos_screen.h"

#include "ash/constants/ash_switches.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/login/guest_tos_screen_handler.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
namespace ash {
namespace {

constexpr const char kUserActionBackClicked[] = "back-button";
constexpr const char kUserActionCancelClicked[] = "cancel";
constexpr const char kUserActionGuestToSAccept[] = "guest-tos-accept";

std::string GetGoogleEulaOnlineUrl() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOobeEulaUrlForTests)) {
    return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kOobeEulaUrlForTests);
  }

  return base::StringPrintf(chrome::kGoogleEulaOnlineURLPath,
                            g_browser_process->GetApplicationLocale().c_str());
}

std::string GetCrosEulaOnlineUrl() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOobeEulaUrlForTests)) {
    return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kOobeEulaUrlForTests);
  }

  return base::StringPrintf(chrome::kCrosEulaOnlineURLPath,
                            g_browser_process->GetApplicationLocale().c_str());
}

}  // namespace

// static
std::string GuestTosScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::ACCEPT:
      return "Accept";
    case Result::BACK:
      return "Back";
    case Result::CANCEL:
      return "Cancel";
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

GuestTosScreen::GuestTosScreen(base::WeakPtr<GuestTosScreenView> view,
                               const ScreenExitCallback& exit_callback)
    : BaseScreen(GuestTosScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {
  DCHECK(view_);
}

GuestTosScreen::~GuestTosScreen() = default;

void GuestTosScreen::ShowImpl() {
  if (!view_)
    return;
  view_->Show(GetGoogleEulaOnlineUrl(), GetCrosEulaOnlineUrl());
}

void GuestTosScreen::HideImpl() {}

void GuestTosScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionBackClicked) {
    exit_callback_.Run(Result::BACK);
  } else if (action_id == kUserActionCancelClicked) {
    exit_callback_.Run(Result::CANCEL);
  } else if (action_id == kUserActionGuestToSAccept) {
    CHECK_EQ(args.size(), 2u);
    const bool enable_usage_stats = args[1].GetBool();
    OnAccept(enable_usage_stats);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

void GuestTosScreen::OnAccept(bool enable_usage_stats) {
  // TODO(crbug/1298249): Add browser tests to ensure that the feature is
  // working.
  PrefService* local_state = g_browser_process->local_state();

  // Store guest consent to local state so that correct metrics consent can be
  // loaded after browser restart.
  local_state->SetBoolean(prefs::kOobeGuestMetricsEnabled, enable_usage_stats);
  local_state->CommitPendingWrite(
      base::BindOnce(&GuestTosScreen::OnOobeGuestPrefWriteDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GuestTosScreen::OnOobeGuestPrefWriteDone() {
  DCHECK(exit_callback_);

  exit_callback_.Run(Result::ACCEPT);
}

}  // namespace ash
