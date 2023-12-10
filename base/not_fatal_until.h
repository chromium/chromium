// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NOT_FATAL_UNTIL_H_
#define BASE_NOT_FATAL_UNTIL_H_

namespace base {

// Add new entries a few milestones into the future whenever necessary.
// M here refers to milestones, see chrome/VERSION's MAJOR field that updates
// when chromium branches.
//
// To clean up old entries remove the already-fatal argument from CHECKs as well
// as from this list. This generates better-optimized CHECKs in official builds.
enum class NotFatalUntil {
  NoSpecifiedMilestoneInternal = -1,
  M120 = 120,
  M121 = 121,
  M122 = 122,
  M123 = 123,
  M124 = 124,
  M125 = 125,
  M126 = 126,
  M127 = 127,
  M128 = 128,
  M129 = 129,
  M130 = 130,
};

}  // namespace base

#endif  // BASE_NOT_FATAL_UNTIL_H_
