// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_FOUNDATION_SERVICE_MONITOR_H_
#define CHROME_BROWSER_MEDIA_MEDIA_FOUNDATION_SERVICE_MONITOR_H_

#include "content/public/browser/service_process_host.h"

class MediaFoundationServiceMonitor
    : public content::ServiceProcessHost::Observer {
 public:
  // Returns the MediaFoundationServiceMonitor singleton.
  static MediaFoundationServiceMonitor* GetInstance();

  MediaFoundationServiceMonitor(const MediaFoundationServiceMonitor&) = delete;
  MediaFoundationServiceMonitor& operator=(
      const MediaFoundationServiceMonitor&) = delete;

  // ServiceProcessHost::Observer implementation.
  void OnServiceProcessCrashed(
      const content::ServiceProcessInfo& info) override;

 private:
  // Make constructor/destructor private since this is a singleton.
  MediaFoundationServiceMonitor();
  ~MediaFoundationServiceMonitor() override;

  int num_crashes_ = 0;
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_FOUNDATION_SERVICE_MONITOR_H_
