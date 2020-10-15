// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_TEST_NOOP_DUAL_MEDIA_SINK_SERVICE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_TEST_NOOP_DUAL_MEDIA_SINK_SERVICE_H_

#include "chrome/browser/media/router/providers/cast/dual_media_sink_service.h"

namespace media_router {

class LoggerImpl;

class NoopDualMediaSinkService : public DualMediaSinkService {
 public:
  NoopDualMediaSinkService();
  ~NoopDualMediaSinkService() override;

  // DualMediaSinkService
  void OnUserGesture() override {}
  void StartMdnsDiscovery() override {}
  void BindLogger(LoggerImpl* logger_impl) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NoopDualMediaSinkService);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_TEST_NOOP_DUAL_MEDIA_SINK_SERVICE_H_
