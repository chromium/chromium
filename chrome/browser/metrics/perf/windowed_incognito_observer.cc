// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/windowed_incognito_observer.h"

#include <tuple>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/public/browser/browser_thread.h"

namespace metrics {

WindowedIncognitoObserver::WindowedIncognitoObserver(
    WindowedIncognitoMonitor* monitor,
    uint64_t num_incognito_window_opened)
    : windowed_incognito_monitor_(monitor),
      num_incognito_window_opened_(num_incognito_window_opened) {}

bool WindowedIncognitoObserver::IncognitoLaunched() const {
  return windowed_incognito_monitor_->IncognitoLaunched(
      num_incognito_window_opened_);
}

bool WindowedIncognitoObserver::IncognitoActive() const {
  return windowed_incognito_monitor_->IncognitoActive();
}

// static
void WindowedIncognitoMonitor::Init() {
  std::ignore = WindowedIncognitoMonitor::Get();
}

// static
std::unique_ptr<WindowedIncognitoObserver>
WindowedIncognitoMonitor::CreateObserver() {
  WindowedIncognitoMonitor* instance = WindowedIncognitoMonitor::Get();
  return instance->CreateIncognitoObserver();
}

// static
WindowedIncognitoMonitor* WindowedIncognitoMonitor::Get() {
  static base::NoDestructor<WindowedIncognitoMonitor> instance;
  return instance.get();
}

WindowedIncognitoMonitor::WindowedIncognitoMonitor()
    : num_active_incognito_windows_(0), num_incognito_window_opened_(0) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  RegisterInstance();
}

WindowedIncognitoMonitor::~WindowedIncognitoMonitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UnregisterInstance();
}

void WindowedIncognitoMonitor::RegisterInstance() {
  // |running_sessions_| is only accessed on the UI thread in RegisterInstance
  // and UnregisterInstance. Therefore, we don't need to explicitly synchronize
  // access to it.
  if (running_sessions_++)
    return;
  BrowserList::AddObserver(this);

  for (Browser* window : *BrowserList::GetInstance()) {
    if (window->profile()->IsOffTheRecord())
      num_active_incognito_windows_++;
  }
}

void WindowedIncognitoMonitor::UnregisterInstance() {
  // |running_sessions_| is only accessed on the UI thread in RegisterInstance
  // and UnregisterInstance. Therefore, we don't need to explicitly synchronize
  // access to it.
  DCHECK_GT(running_sessions_, 0);
  if (!--running_sessions_)
    BrowserList::RemoveObserver(this);
}

std::unique_ptr<WindowedIncognitoObserver>
WindowedIncognitoMonitor::CreateIncognitoObserver() {
  base::AutoLock lock(lock_);
  return std::make_unique<WindowedIncognitoObserver>(
      this, num_incognito_window_opened_);
}

bool WindowedIncognitoMonitor::IncognitoActive() const {
  base::AutoLock lock(lock_);
  return num_active_incognito_windows_ > 0;
}

bool WindowedIncognitoMonitor::IncognitoLaunched(
    uint64_t prev_num_incognito_opened) const {
  base::AutoLock lock(lock_);
  // Whether there is any incognito window opened after the observer was
  // created.
  return prev_num_incognito_opened < num_incognito_window_opened_;
}

void WindowedIncognitoMonitor::OnBrowserAdded(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!browser->profile()->IsOffTheRecord())
    return;

  base::AutoLock lock(lock_);
  num_active_incognito_windows_++;
  num_incognito_window_opened_++;
}

void WindowedIncognitoMonitor::OnBrowserRemoved(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!browser->profile()->IsOffTheRecord())
    return;

  base::AutoLock lock(lock_);
  DCHECK(num_active_incognito_windows_ > 0);
  num_active_incognito_windows_--;
}

}  // namespace metrics
