// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_STREAM_PROVIDER_H_
#define CHROME_BROWSER_DICTATION_STREAM_PROVIDER_H_

namespace dictation {

class Target;

// An interface to a Dictation StreamProvider which provides user-dicatated text
// input.
class StreamProvider {
 public:
  virtual ~StreamProvider() = default;

  // Sets the target that the stream provider's output will be committed to.
  virtual void BindToTarget(Target& target) = 0;

  // Requests the stream provider to stop listening and transcribing.
  virtual void Stop() = 0;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_STREAM_PROVIDER_H_
