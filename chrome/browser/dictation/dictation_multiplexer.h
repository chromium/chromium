// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_DICTATION_MULTIPLEXER_H_
#define CHROME_BROWSER_DICTATION_DICTATION_MULTIPLEXER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/types/id_type.h"
#include "chrome/browser/dictation/stream_provider.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace dictation {

// DictationMultiplexer maps stream IDs to StreamProviders and forwards
// transcription updates and state changes to the appropriate provider.
class DictationMultiplexer {
 public:
  DictationMultiplexer();
  ~DictationMultiplexer();

  DictationMultiplexer(const DictationMultiplexer&) = delete;
  DictationMultiplexer& operator=(const DictationMultiplexer&) = delete;

  using StreamId = base::IdType32<class StreamIdTag>;

  // Generates a unique stream ID.
  StreamId GenerateStreamId();

  // Forwards transcription update to the StreamProvider associated with
  // `stream_id`. Returns true if a matching provider was found.
  bool UpdateTranscription(StreamId stream_id,
                           const std::string& data,
                           bool is_final);

  // Forwards stream state change to the StreamProvider associated with
  // `stream_id`. Returns true if a matching provider was found.
  bool SetStreamState(StreamId stream_id,
                      StreamProvider::StreamState state,
                      StreamErrorReason reason);

  // Associates a StreamProvider with a stream ID.
  // The multiplexer does not own the provider.
  void RegisterStreamProvider(StreamId stream_id,
                              StreamProvider* stream_provider);
  void UnregisterStreamProvider(StreamId stream_id);

 private:
  StreamId::Generator generator_;
  absl::flat_hash_map<StreamId, raw_ptr<StreamProvider>> stream_providers_;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_DICTATION_MULTIPLEXER_H_
