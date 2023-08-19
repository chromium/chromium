// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_observer.h"

#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager_lacros.h"
#endif

namespace policy {

namespace {
static DlpContentObserver* g_dlp_content_observer = nullptr;
}  // namespace

// static
DlpContentObserver* DlpContentObserver::Get() {
  if (g_dlp_content_observer)
    return g_dlp_content_observer;

    // Initializes DlpContentManager(Ash/Lacros) if needed.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* manager = new DlpContentManagerAsh();
  manager->Init();
  g_dlp_content_observer = manager;
#else
  auto* manager = new DlpContentManagerLacros();
  manager->Init();
  g_dlp_content_observer = manager;
#endif
  return g_dlp_content_observer;
}

// static
bool DlpContentObserver::HasInstance() {
  return g_dlp_content_observer != nullptr;
}

/* static */
void DlpContentObserver::SetDlpContentObserverForTesting(
    DlpContentObserver* dlp_content_observer) {
  if (g_dlp_content_observer)
    delete g_dlp_content_observer;
  g_dlp_content_observer = dlp_content_observer;
}

/* static */
void DlpContentObserver::ResetDlpContentObserverForTesting() {
  g_dlp_content_observer = nullptr;
}

// ScopedDlpContentObserverForTesting
ScopedDlpContentObserverForTesting::ScopedDlpContentObserverForTesting(
    DlpContentObserver* test_dlp_content_observer) {
  DlpContentObserver::SetDlpContentObserverForTesting(
      test_dlp_content_observer);
}

ScopedDlpContentObserverForTesting::~ScopedDlpContentObserverForTesting() {
  DlpContentObserver::ResetDlpContentObserverForTesting();
}

}  // namespace policy
