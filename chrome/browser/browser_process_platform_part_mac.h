// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_MAC_H_
#define CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_MAC_H_

#include <memory>

#include "chrome/browser/apps/app_shim/app_shim_listener.h"
#include "chrome/browser/browser_process_platform_part_base.h"
#include "chrome/browser/mac/key_window_notifier.h"

namespace apps {
class AppShimManager;
}  // namespace apps

class BrowserProcessPlatformPart : public BrowserProcessPlatformPartBase {
 public:
  BrowserProcessPlatformPart();

  BrowserProcessPlatformPart(const BrowserProcessPlatformPart&) = delete;
  BrowserProcessPlatformPart& operator=(const BrowserProcessPlatformPart&) =
      delete;

  ~BrowserProcessPlatformPart() override;

  // Overridden from BrowserProcessPlatformPartBase:
  void StartTearDown() override;
  void AttemptExit(bool try_to_quit_application) override;
  void PreMainMessageLoopRun() override;

  AppShimListener* app_shim_listener();
  apps::AppShimManager* app_shim_manager();

  KeyWindowNotifier& key_window_notifier() { return key_window_notifier_; }

 private:
  std::unique_ptr<apps::AppShimManager> app_shim_manager_;

  // Hosts the IPC channel factory that App Shims connect to on Mac.
  scoped_refptr<AppShimListener> app_shim_listener_;

  KeyWindowNotifier key_window_notifier_;
};

#endif  // CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_MAC_H_
