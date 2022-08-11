// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_STATE_H_
#define CHROME_BROWSER_DIPS_DIPS_STATE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class DIPSStorage;

// A boolean value that gets cleared when moved.
class DirtyBit {
 public:
  explicit DirtyBit(bool value = false) : value_(value) {}
  DirtyBit(DirtyBit&& old) : value_(std::exchange(old.value_, false)) {}

  explicit operator bool() const { return value_; }

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
  DIPSState(DIPSStorage* storage, std::string site, bool was_loaded);
  DIPSState(DIPSState&&);
  // Flushes changes to storage_.
  ~DIPSState();

  const std::string& site() const { return site_; }
  // True iff this DIPSState was loaded from DIPSStorage (as opposed to being
  // default-initialized for a new site).
  bool was_loaded() const { return was_loaded_; }

  absl::optional<base::Time> site_storage_time() const {
    return site_storage_time_;
  }
  void set_site_storage_time(absl::optional<base::Time> time);

  absl::optional<base::Time> user_interaction_time() const {
    return user_interaction_time_;
  }
  void set_user_interaction_time(absl::optional<base::Time> time);

 private:
  raw_ptr<DIPSStorage> storage_;
  std::string site_;
  bool was_loaded_;
  DirtyBit dirty_;
  absl::optional<base::Time> site_storage_time_;
  absl::optional<base::Time> user_interaction_time_;
};

#endif  // CHROME_BROWSER_DIPS_DIPS_STATE_H_
