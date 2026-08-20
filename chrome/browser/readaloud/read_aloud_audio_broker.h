// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READALOUD_READ_ALOUD_AUDIO_BROKER_H_
#define CHROME_BROWSER_READALOUD_READ_ALOUD_AUDIO_BROKER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/unguessable_token.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace readaloud {

// ReadAloudAudioBroker is responsible for broker-side audio stream creation in
// the privileged Browser process on behalf of the sandboxed ReadAloud playback
// utility process. This enforces the least-privilege security boundary (Rule of
// Two) by ensuring the untrusted/sandboxed utility process cannot directly
// access audio hardware or factory interfaces.
//
// Threading & Lifetime:
// Must be created, used, and destroyed on the UI thread. Owned 1:1 by
// ReadAloudService.
class ReadAloudAudioBroker {
 public:
  using AudioStreamFactoryBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory>)>;

  using CreateStreamCallback = base::OnceCallback<void(
      mojo::PendingRemote<media::mojom::AudioOutputStream> stream_remote,
      media::mojom::ReadWriteAudioDataPipePtr data_pipe)>;

  // Initializes the audio broker. `factory_binder` can be provided in tests to
  // inject a mock or fake AudioStreamFactory. If empty, defaults to
  // content::GetAudioServiceStreamFactoryBinder().
  explicit ReadAloudAudioBroker(
      AudioStreamFactoryBinder factory_binder = AudioStreamFactoryBinder());

  ReadAloudAudioBroker(const ReadAloudAudioBroker&) = delete;
  ReadAloudAudioBroker& operator=(const ReadAloudAudioBroker&) = delete;

  ~ReadAloudAudioBroker();

  // Requests creation of an AudioOutputStream with the Audio Service.
  // `group_id` is the tab/session unguessable token for muting/capturing.
  // `params` contains the desired sample rate, channel layout, and buffer size.
  // `callback` is invoked when the Audio Service responds with the stream
  // remote and data pipe (or null data pipe on failure).
  void CreateOutputStream(const base::UnguessableToken& group_id,
                          const media::AudioParameters& params,
                          CreateStreamCallback callback);

  // Cancels any in-flight stream creation callbacks and resets the factory
  // remote.
  void Reset();

 private:
  void EnsureFactoryConnected();
  void OnFactoryDisconnect();
  void OnStreamCreated(
      mojo::PendingRemote<media::mojom::AudioOutputStream> stream_remote,
      CreateStreamCallback callback,
      media::mojom::ReadWriteAudioDataPipePtr data_pipe);

  AudioStreamFactoryBinder factory_binder_;
  mojo::Remote<media::mojom::AudioStreamFactory> stream_factory_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ReadAloudAudioBroker> weak_factory_{this};
};

}  // namespace readaloud

#endif  // CHROME_BROWSER_READALOUD_READ_ALOUD_AUDIO_BROKER_H_
