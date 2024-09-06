// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/application_lifetime.h"

#include <memory>
#include <set>
#include <string>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/aura/env.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace chrome {

namespace {

void AttemptExitInternal(bool try_to_quit_application) {
  // On Mac, the platform-specific part handles setting this.
#if !BUILDFLAG(IS_MAC)
  if (try_to_quit_application) {
    browser_shutdown::SetTryingToQuit(true);
  }
#endif  // !BUILDFLAG(IS_MAC)

#if !BUILDFLAG(IS_ANDROID)
  OnClosingAllBrowsers(true);
#endif  // !BUILDFLAG(IS_ANDROID)

  g_browser_process->platform_part()->AttemptExit(try_to_quit_application);
}

}  // namespace

// The ChromeOS implementations are in application_lifetime_chromeos.cc
#if !BUILDFLAG(IS_CHROMEOS_ASH)

void AttemptUserExit() {
  // Reset the restart bit that might have been set in cancelled restart
  // request.
#if !BUILDFLAG(IS_ANDROID)
  ProfilePicker::Hide();
#endif  // !BUILDFLAG(IS_ANDROID)
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kRestartLastSessionOnShutdown, false);
  AttemptExitInternal(false);
}

void AttemptRelaunch() {
  AttemptRestart();
}

void AttemptExit() {
  // If we know that all browsers can be closed without blocking,
  // don't notify users of crashes beyond this point.
  // Note that MarkAsCleanShutdown() does not set UMA's exit cleanly bit
  // so crashes during shutdown are still reported in UMA.
#if !BUILDFLAG(IS_ANDROID)
  // Android doesn't use Browser.
  if (AreAllBrowsersCloseable()) {
    MarkAsCleanShutdown();
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  AttemptExitInternal(true);
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

void ExitIgnoreUnloadHandlers() {
  VLOG(1) << "ExitIgnoreUnloadHandlers";
#if !BUILDFLAG(IS_ANDROID)
  // We always mark exit cleanly.
  MarkAsCleanShutdown();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Disable window occlusion tracking on exit before closing all browser
  // windows to make shutdown faster. Note that the occlusion tracking is
  // paused indefinitely. It is okay do so on Chrome OS because there is
  // no way to abort shutdown and go back to user sessions at this point.
  DCHECK(aura::Env::HasInstance());
  aura::Env::GetInstance()->PauseWindowOcclusionTracking();

  // On ChromeOS ExitIgnoreUnloadHandlers() is used to handle SIGTERM.
  // In this case, AreAllBrowsersCloseable()
  // can be false in following cases. a) power-off b) signout from
  // screen locker.
  browser_shutdown::OnShutdownStarting(
      AreAllBrowsersCloseable() ? browser_shutdown::ShutdownType::kBrowserExit
                                : browser_shutdown::ShutdownType::kEndSession);
#else  // !BUILDFLAG(IS_CHROMEOS_ASH)
  // For desktop browsers, always perform a silent exit.
  browser_shutdown::OnShutdownStarting(
      browser_shutdown::ShutdownType::kSilentExit);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // !BUILDFLAG(IS_ANDROID)
  AttemptExitInternal(true);
}

}  // namespace chrome
