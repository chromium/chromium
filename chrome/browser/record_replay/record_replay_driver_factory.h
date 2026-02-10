// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RECORD_REPLAY_RECORD_REPLAY_DRIVER_FACTORY_H_
#define CHROME_BROWSER_RECORD_REPLAY_RECORD_REPLAY_DRIVER_FACTORY_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace record_replay {

class RecordReplayClient;
class RecordReplayDriver;

// This class manages the lifecycle of RecordReplayDriver instances.
//
// Owned by RecordReplayClient.
class RecordReplayDriverFactory : public content::WebContentsObserver {
 public:
  explicit RecordReplayDriverFactory(RecordReplayClient& client);
  RecordReplayDriverFactory(const RecordReplayDriverFactory&) = delete;
  RecordReplayDriverFactory& operator=(const RecordReplayDriverFactory&) =
      delete;
  ~RecordReplayDriverFactory() override;

  using content::WebContentsObserver::Observe;

  RecordReplayDriver* GetOrCreateDriver(content::RenderFrameHost* rfh);
  RecordReplayDriver* GetDriver(const blink::LocalFrameToken& frame_token);

  // Returns all drivers whose frame is active. That is, it does not include
  // drivers whose frame is bfcached or prerendered.
  std::vector<RecordReplayDriver*> GetActiveDrivers();

  // Calls `fun` for all drivers, including inactive ones.
  void ForEachDriver(base::FunctionRef<void(RecordReplayDriver&)> fun);

  // Enables or disables recording for all drivers that will be created from on.
  void SetRecordForFutureDrivers(bool enable);

 private:
  // content::WebContentsObserver implementation.
  void RenderFrameCreated(content::RenderFrameHost* rfh) override;
  void RenderFrameDeleted(content::RenderFrameHost* rfh) override;

  const raw_ref<RecordReplayClient> client_;
  absl::flat_hash_map<blink::LocalFrameToken,
                      std::unique_ptr<RecordReplayDriver>>
      drivers_;
  bool record_future_drivers_ = false;
};

}  // namespace record_replay

#endif  // CHROME_BROWSER_RECORD_REPLAY_RECORD_REPLAY_DRIVER_FACTORY_H_
