// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_LISTENER_STREAM_PROVIDER_H_
#define CHROME_BROWSER_DICTATION_LISTENER_STREAM_PROVIDER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/dictation/dictation_context.h"
#include "chrome/browser/dictation/dictation_multiplexer.h"
#include "chrome/browser/dictation/stream_provider.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace dictation {
class DictationContextFetcher;
class StreamProviderDelegate;
class Target;

// A StreamProvider implementation that listens to dictation input via the
// dictation private extension API.
class ListenerStreamProvider : public StreamProvider {
 public:
  explicit ListenerStreamProvider(content::BrowserContext* browser_context,
                                  StreamProviderDelegate& delegate);
  ~ListenerStreamProvider() override;

  ListenerStreamProvider(const ListenerStreamProvider&) = delete;
  ListenerStreamProvider& operator=(const ListenerStreamProvider&) = delete;

  // StreamProvider:
  void BindToTargetAndConnect(std::unique_ptr<Target> target) override;
  void Stop() override;
  void OnTranscriptionUpdated(const std::string& data, bool is_final) override;
  void OnStreamStateChanged(StreamState state) override;
  StreamState GetState() const override;
  Target* GetTarget() override;
  const Target* GetTarget() const override;

  const std::string& GetLatestTranscriptionForTesting() const;
  bool IsTranscriptionFinalForTesting() const;
  DictationMultiplexer::StreamId stream_id_for_testing() const {
    return stream_id_;
  }
  base::WeakPtr<ListenerStreamProvider> GetWeakPtr();

 private:
  DictationMultiplexer& GetMultiplexer() const;
  void StartStream(std::optional<DictationContext> context);
  void OnStartContextCaptured(DictationContext result);
  void OnAsyncContextCaptured(DictationContext result);
  void OnPendingInsertionsComplete();

  // Owns this
  const base::raw_ref<StreamProviderDelegate> delegate_;
  std::unique_ptr<Target> target_;
  raw_ptr<content::BrowserContext> browser_context_;

  bool needs_end_stream_ = false;
  DictationMultiplexer::StreamId stream_id_;
  std::string latest_transcription_;
  StreamState state_ = StreamState::kInitializing;

  bool is_final_for_testing_ = false;

  std::unique_ptr<DictationContextFetcher> context_fetcher_;

  base::WeakPtrFactory<ListenerStreamProvider> weak_ptr_factory_{this};
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_LISTENER_STREAM_PROVIDER_H_
