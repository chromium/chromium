// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_STREAM_PROVIDER_DELEGATE_H_
#define CHROME_BROWSER_DICTATION_STREAM_PROVIDER_DELEGATE_H_

#include "chrome/browser/dictation/stream_provider.h"

namespace dictation {

// Interface for StreamProvider to communicate state updates to the session
// controller.
class StreamProviderDelegate {
 public:
  virtual ~StreamProviderDelegate() = default;

  // Called when the stream provider state has been updated.
  virtual void DidUpdateStreamProviderState(
      StreamProvider& stream_provider,
      StreamProvider::StreamState old_state,
      StreamErrorReason reason = StreamErrorReason::kNone) = 0;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_STREAM_PROVIDER_DELEGATE_H_
