// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_SETTINGS_H_
#define CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_SETTINGS_H_

#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class FirstPartySetsSettings {
 public:
  static FirstPartySetsSettings* Get();

  FirstPartySetsSettings() = default;
  // Note that FirstPartySetsSettings is a singleton that's never destroyed.
  ~FirstPartySetsSettings() = delete;
  FirstPartySetsSettings(const FirstPartySetsSettings&) = delete;
  FirstPartySetsSettings& operator=(const FirstPartySetsSettings&) = delete;

  bool IsFirstPartySetsEnabled();
  void ResetForTesting();

 private:
  friend class base::NoDestructor<FirstPartySetsSettings>;

  absl::optional<bool> enabled_ GUARDED_BY_CONTEXT(sequence_checker_) =
      absl::nullopt;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_SETTINGS_H_