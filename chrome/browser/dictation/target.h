// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_TARGET_H_
#define CHROME_BROWSER_DICTATION_TARGET_H_

namespace dictation {

// Represents a dictation target into which transcriptions will be written.
class Target {
 public:
  virtual ~Target() = default;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_TARGET_H_
