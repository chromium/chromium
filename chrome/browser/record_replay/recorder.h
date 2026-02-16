// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RECORD_REPLAY_RECORDER_H_
#define CHROME_BROWSER_RECORD_REPLAY_RECORDER_H_

#include <string>

#include "base/time/time.h"
#include "chrome/browser/record_replay/recording.pb.h"

class GURL;

namespace record_replay {

// Builds a `Recording` from a series of actions.
class Recorder {
 public:
  explicit Recorder(GURL start_url, base::Time start_time = base::Time::Now());
  Recorder(const Recorder&) = delete;
  Recorder& operator=(const Recorder&) = delete;
  ~Recorder();

  GURL start_url() const;
  base::Time start_time() const;

  void AddClick(std::string element_selector);
  void AddSelectChange(std::string element_selector, std::string value);
  void AddTextChange(std::string element_selector, std::string text);
  void AddAutofill(std::string element_selector,
                   Recording::Action::AutofillSpecifics::Type type,
                   std::string guid);

  const Recording& recording() const { return recording_; }

 private:
  // Sets the `action`'s delta and updates `last_time_`.
  void UpdateDelta(Recording::Action& action);

  // The recording under construction.
  Recording recording_;
  // The time of the last action or, if there are no actions, the start time of
  // the recording.
  base::Time last_time_;
};

}  // namespace record_replay

#endif  // CHROME_BROWSER_RECORD_REPLAY_RECORDER_H_
