// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_TEST_NOOP_DUAL_MEDIA_SINK_SERVICE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_TEST_NOOP_DUAL_MEDIA_SINK_SERVICE_H_

#include "build/build_config.h"
#include "chrome/browser/media/router/providers/cast/dual_media_sink_service.h"

namespace media_router {

class NoopDualMediaSinkService : public DualMediaSinkService {
 public:
  NoopDualMediaSinkService();

  NoopDualMediaSinkService(const NoopDualMediaSinkService&) = delete;
  NoopDualMediaSinkService& operator=(const NoopDualMediaSinkService&) = delete;

  ~NoopDualMediaSinkService() override;

  // DualMediaSinkService
  void DiscoverSinksNow() override {}
#if BUILDFLAG(IS_WIN)
  void StartMdnsDiscovery() override {}
#endif
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_TEST_NOOP_DUAL_MEDIA_SINK_SERVICE_H_
