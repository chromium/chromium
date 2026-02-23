// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/recorder.h"

#include <string>

#include "base/check.h"
#include "base/strings/string_view_util.h"
#include "base/time/time.h"
#include "chrome/browser/record_replay/recording.pb.h"
#include "chrome/common/record_replay/aliases.h"
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

void Recorder::AddClick(Selector element_selector) {
  Recording::Action& action = *recording_.mutable_actions()->Add();
  action.set_element_selector(*std::move(element_selector));
  action.mutable_click_specifics();
  UpdateDelta(action);
}

void Recorder::AddSelectChange(Selector element_selector, FieldValue value) {
  Recording::Action& action = *recording_.mutable_actions()->Add();
  action.set_element_selector(*std::move(element_selector));
  action.mutable_select_specifics()->set_value(*std::move(value));
  UpdateDelta(action);
}

void Recorder::AddTextChange(Selector element_selector, FieldValue text) {
  Recording::Action& action = *recording_.mutable_actions()->Add();
  action.set_element_selector(*std::move(element_selector));
  action.mutable_text_specifics()->set_value(*std::move(text));
  UpdateDelta(action);
}

void Recorder::AddAutofill(Selector element_selector,
                           Recording::Action::AutofillSpecifics::Type type,
                           std::string guid) {
  Recording::Action& action = *recording_.mutable_actions()->Add();
  action.set_element_selector(*std::move(element_selector));
  action.mutable_autofill_specifics()->set_type(type);
  action.mutable_autofill_specifics()->set_guid(std::move(guid));
  UpdateDelta(action);
}

void Recorder::SetName(std::string name) {
  recording_.set_name(std::move(name));
}

void Recorder::SetScreenshot(base::span<const uint8_t> screenshot) {
  recording_.set_screenshot(base::as_string_view(screenshot));
}

}  // namespace record_replay
