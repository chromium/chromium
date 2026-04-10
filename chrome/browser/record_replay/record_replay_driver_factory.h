// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RECORD_REPLAY_RECORD_REPLAY_DRIVER_FACTORY_H_
#define CHROME_BROWSER_RECORD_REPLAY_RECORD_REPLAY_DRIVER_FACTORY_H_

#include <vector>

#include "base/functional/function_ref.h"
#include "chrome/browser/record_replay/element_id.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/render_frame_host.h"

namespace record_replay {

class RecordReplayDriver;

// This class manages the lifecycle of RecordReplayDriver instances within a
// tab.
class RecordReplayDriverFactory {
 public:
  virtual ~RecordReplayDriverFactory() = 0;

  virtual RecordReplayDriver* GetDriver(const ElementId& element_id) = 0;
  virtual RecordReplayDriver* GetDriver(
      const autofill::FieldGlobalId& field_id) = 0;

  // Returns all drivers whose frame is active. That is, it does not include
  // drivers whose frame is bfcached or prerendered.
  virtual std::vector<RecordReplayDriver*> GetActiveDrivers() = 0;

  // Calls `fun` for all drivers, including inactive ones.
  virtual void ForEachDriver(
      base::FunctionRef<void(RecordReplayDriver&)> fun) = 0;

  // Enables or disables recording for all drivers that will be created from on.
  virtual void SetRecordForFutureDrivers(bool enable) = 0;
};

inline RecordReplayDriverFactory::~RecordReplayDriverFactory() {}

}  // namespace record_replay

#endif  // CHROME_BROWSER_RECORD_REPLAY_RECORD_REPLAY_DRIVER_FACTORY_H_
