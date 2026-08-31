// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_multiplexer.h"

#include "base/check.h"

namespace dictation {

DictationMultiplexer::DictationMultiplexer() = default;

DictationMultiplexer::~DictationMultiplexer() = default;

DictationMultiplexer::StreamId DictationMultiplexer::GenerateStreamId() {
  return generator_.GenerateNextId();
}

bool DictationMultiplexer::UpdateTranscription(StreamId stream_id,
                                               const std::string& data,
                                               bool is_final) {
  auto it = stream_providers_.find(stream_id);
  if (it == stream_providers_.end()) {
    return false;
  }
  it->second->OnTranscriptionUpdated(data, is_final);
  return true;
}

bool DictationMultiplexer::SetStreamState(StreamId stream_id,
                                          StreamProvider::StreamState state,
                                          StreamErrorReason reason) {
  auto it = stream_providers_.find(stream_id);
  if (it == stream_providers_.end()) {
    return false;
  }
  it->second->OnStreamStateChanged(state, reason);
  return true;
}

void DictationMultiplexer::RegisterStreamProvider(
    StreamId stream_id,
    StreamProvider* stream_provider) {
  CHECK(stream_provider);
  stream_providers_[stream_id] = stream_provider;
}

void DictationMultiplexer::UnregisterStreamProvider(StreamId stream_id) {
  stream_providers_.erase(stream_id);
}

}  // namespace dictation
