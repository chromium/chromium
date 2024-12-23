// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_STATE_H_
#define CHROME_BROWSER_DIPS_DIPS_STATE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/dips/dips_utils.h"

class DIPSStorage;

// A boolean value that gets cleared when moved.
class DirtyBit {
 public:
  explicit DirtyBit(bool value = false) : value_(value) {}
  DirtyBit(DirtyBit&& old) : value_(std::exchange(old.value_, false)) {}

  explicit operator bool() const { return value_; }

  DirtyBit& operator|=(bool value) {
    value_ |= value;
    return *this;
  }

  DirtyBit& operator=(bool value) {
    value_ = value;
    return *this;
  }

 private:
  bool value_;
};

// Not to be confused with state stored by sites (e.g. cookies, local storage),
// DIPSState represents the state recorded by DIPSService itself.
class DIPSState {
 public:
  DIPSState(DIPSStorage* storage, std::string site);
  // For loaded DIPSState.
  DIPSState(DIPSStorage* storage, std::string site, const StateValue& state);

  DIPSState(DIPSState&&);
  // Flushes changes to storage_.
  ~DIPSState();

  const std::string& site() const { return site_; }
  // True iff this DIPSState was loaded from DIPSStorage (as opposed to being
  // default-initialized for a new site).
  bool was_loaded() const { return was_loaded_; }

  TimestampRange site_storage_times() const {
    return state_.site_storage_times;
  }
  TimestampRange user_interaction_times() const {
    return state_.user_interaction_times;
  }
  TimestampRange stateful_bounce_times() const {
    return state_.stateful_bounce_times;
  }
  TimestampRange bounce_times() const { return state_.bounce_times; }
  TimestampRange web_authn_assertion_times() const {
    return state_.web_authn_assertion_times;
  }

  void update_site_storage_time(base::Time time);
  void update_user_interaction_time(base::Time time);
  void update_stateful_bounce_time(base::Time time);
  void update_bounce_time(base::Time time);
  void update_web_authn_assertion_time(base::Time time);
  StateValue ToStateValue() const { return state_; }

 private:
  raw_ptr<DIPSStorage, AcrossTasksDanglingUntriaged> storage_;
  std::string site_;
  bool was_loaded_;
  DirtyBit dirty_;
  StateValue state_;
};

#endif  // CHROME_BROWSER_DIPS_DIPS_STATE_H_
