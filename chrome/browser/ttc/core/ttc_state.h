// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_TTC_STATE_H_
#define CHROME_BROWSER_TTC_CORE_TTC_STATE_H_

namespace ttc {

enum class TtcState {
  // TTC is unavailable for this profile.
  kDisabled,
  // TTC is available but no session is in progress.
  kSessionInactive,
  // A session is in progress.
  kSessionActive,
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_TTC_STATE_H_
