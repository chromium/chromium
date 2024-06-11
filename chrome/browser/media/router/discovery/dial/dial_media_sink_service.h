// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_MEDIA_SINK_SERVICE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_MEDIA_SINK_SERVICE_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/discovery/media_sink_service_util.h"
#include "url/origin.h"

namespace media_router {

class DialMediaSinkServiceImpl;

using OnDialSinkAddedCallback =
    base::RepeatingCallback<void(const MediaSinkInternal&)>;

// Service to discover DIAL media sinks. All public methods must be invoked on
// the UI thread. Delegates to DialMediaSinkServiceImpl by posting tasks to its
// SequencedTaskRunner. It is owned by a singleton that is never freed.
// TODO(imcheng): Remove this class and moving the logic into a part
// of DialMediaSinkServiceImpl that runs on the UI thread, and renaming
// DialMediaSinkServiceImpl to DialMediaSinkService.
class DialMediaSinkService {
 public:
  DialMediaSinkService();

  DialMediaSinkService(const DialMediaSinkService&) = delete;
  DialMediaSinkService& operator=(const DialMediaSinkService&) = delete;

  virtual ~DialMediaSinkService();

  // Initialize the `DialDiscoveryServiceImpl` for discovery of DIAL sinks but
  // device discovery isn't started until `StartDialDiscovery()` is called. Can
  // only be called once.
  // `sink_discovery_cb`: Callback to invoke on UI thread when the list of
  // discovered sinks has been updated.
  // Marked virtual for tests.
  virtual void Initialize(const OnSinksDiscoveredCallback& sink_discovery_cb);

  // Sets up network service for discovery and starts periodic discovery timer.
  // Might be called multiple times and no-op if discovery has started.
  void StartDiscovery();

  // Starts a new round of discovery cycle. No-op if `StartDialDiscovery()`
  // hasn't been called before.
  virtual void DiscoverSinksNow();

  bool DiscoveryStarted() const { return discovery_started_; }

  // Returns a raw pointer to `impl_`. This method is only valid to call after
  // `Initialize()` has been called. Always returns non-null.
  DialMediaSinkServiceImpl* impl() {
    DCHECK(impl_);
    return impl_.get();
  }

 private:
  // Marked virtual for tests.
  virtual std::unique_ptr<DialMediaSinkServiceImpl, base::OnTaskRunnerDeleter>
  CreateImpl(const OnSinksDiscoveredCallback& sink_discovery_cb);

  void RunSinksDiscoveredCallback(
      const OnSinksDiscoveredCallback& sinks_discovered_cb,
      std::vector<MediaSinkInternal> sinks);

  // Created on the UI thread, used and destroyed on its
  // SequencedTaskRunner.
  std::unique_ptr<DialMediaSinkServiceImpl, base::OnTaskRunnerDeleter> impl_;

  bool discovery_started_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<DialMediaSinkService> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_MEDIA_SINK_SERVICE_H_
