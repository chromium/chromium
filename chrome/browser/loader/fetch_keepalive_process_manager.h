// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOADER_FETCH_KEEPALIVE_PROCESS_MANAGER_H_
#define CHROME_BROWSER_LOADER_FETCH_KEEPALIVE_PROCESS_MANAGER_H_

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

class Profile;
class ScopedKeepAlive;
class ScopedProfileKeepAlive;

// Keeps the browser process alive while browser-side fetch keepalive /
// fetchLater loaders are in flight.
//
// The in-browser migration (blink::features::kKeepAliveInBrowserMigration)
// moves fetch keepalive request handling into the browser-side
// KeepAliveURLLoader so requests survive renderer and page teardown, but the
// loader still dies with the browser process: on desktop the process may exit
// once the last window closes, aborting in-flight keepalive/fetchLater
// requests. This holds a ScopedKeepAlive while any such loader exists. Unlike
// the legacy timer-based hold in
// ChromeContentBrowserClient::OnKeepaliveRequestStarted(), this hold is scoped
// to the loaders' lifetime, which is bounded by KeepAliveURLLoader's own
// timeout, so shutdown cannot be blocked indefinitely.
//
// Each BrowserContext with in-flight loaders additionally holds a
// ScopedProfileKeepAlive for its original profile, so a profile is not
// destroyed -- tearing down its StoragePartition and aborting its loaders --
// while it still has work in flight.
//
// Desktop only; not built on Android.
class FetchKeepAliveProcessManager {
 public:
  FetchKeepAliveProcessManager();
  ~FetchKeepAliveProcessManager();

  FetchKeepAliveProcessManager(const FetchKeepAliveProcessManager&) = delete;
  FetchKeepAliveProcessManager& operator=(const FetchKeepAliveProcessManager&) =
      delete;

  // Called when a fetch keepalive loader is created for `profile`.
  void OnRequestCreated(Profile& profile);

  // Called when a previously-created loader for `profile` is destroyed.
  void OnRequestDestroyed(Profile& profile);

 private:
  // Per-Profile hold. Keys are only used as identifiers and are never
  // dereferenced during request destruction -- a profile may already be
  // partway through destruction when its loaders are torn down with its
  // StoragePartition. Entries are erased when their loader count drops to zero.
  struct ProfileHold {
    uint64_t num_loaders = 0;
    std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive;
  };

  uint64_t num_loaders_ = 0;
  std::unique_ptr<ScopedKeepAlive> process_keep_alive_;
  absl::flat_hash_map<raw_ptr<Profile>, ProfileHold> profile_holds_;
  base::TimeTicks hold_started_;
  uint64_t max_concurrent_ = 0;
};

#endif  // CHROME_BROWSER_LOADER_FETCH_KEEPALIVE_PROCESS_MANAGER_H_
