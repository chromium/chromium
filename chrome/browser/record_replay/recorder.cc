// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/recorder.h"

#include <string>

#include "base/check.h"
#include "base/time/time.h"
#include "chrome/browser/record_replay/recording.pb.h"
#include "url/gurl.h"

namespace record_replay {

Recorder::Recorder(GURL start_url, base::Time start_time)
    : last_time_(start_time) {
  recording_.set_url(start_url.spec());
  recording_.set_start_time(
      start_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

Recorder::~Recorder() = default;

GURL Recorder::start_url() const {
  return GURL(recording_.url());
}

base::Time Recorder::start_time() const {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(recording_.start_time()));
}

void Recorder::UpdateDelta(Recording::Action& action) {
  DCHECK(!recording_.actions().empty());
  const base::Time now = base::Time::Now();
  action.set_delta((now - last_time_).InMicroseconds());
  last_time_ = now;
}

void Recorder::AddClick(std::string element_selector) {
  Recording::Action& action = *recording_.mutable_actions()->Add();
  action.set_element_selector(std::move(element_selector));
  action.mutable_click_specifics();
  UpdateDelta(action);
}

void Recorder::AddSelectChange(std::string element_selector,
                               std::string value) {
  Recording::Action& action = *recording_.mutable_actions()->Add();
  action.set_element_selector(std::move(element_selector));
  action.mutable_select_specifics()->set_value(std::move(value));
  UpdateDelta(action);
}

void Recorder::AddTextChange(std::string element_selector, std::string text) {
  Recording::Action& action = *recording_.mutable_actions()->Add();
  action.set_element_selector(std::move(element_selector));
  action.mutable_text_specifics()->set_value(std::move(text));
  UpdateDelta(action);
}

void Recorder::AddAutofill(std::string element_selector,
                           Recording::Action::AutofillSpecifics::Type type,
                           std::string guid) {
  Recording::Action& action = *recording_.mutable_actions()->Add();
  action.set_element_selector(std::move(element_selector));
  action.mutable_autofill_specifics()->set_type(type);
  action.mutable_autofill_specifics()->set_guid(std::move(guid));
  UpdateDelta(action);
}

}  // namespace record_replay
