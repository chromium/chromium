// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READALOUD_FAKE_AUDIO_STREAM_FACTORY_H_
#define CHROME_BROWSER_READALOUD_FAKE_AUDIO_STREAM_FACTORY_H_

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/audio/public/cpp/fake_stream_factory.h"

namespace readaloud {

class FakeAudioStreamFactory : public audio::FakeStreamFactory {
 public:
  FakeAudioStreamFactory();
  ~FakeAudioStreamFactory() override;

  void Bind(mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver);
  void Disconnect();

  void CreateOutputStream(
      mojo::PendingReceiver<media::mojom::AudioOutputStream> stream,
      mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
          observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      const std::string& device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      CreateOutputStreamCallback callback) override;

  void RespondWithLastCallback(bool succeed);

  static media::mojom::ReadWriteAudioDataPipePtr CreateValidDataPipe(
      const media::AudioParameters& params);

  void set_create_output_stream_callback(base::OnceClosure callback) {
    create_output_stream_callback_ = std::move(callback);
  }

  void set_auto_respond(bool auto_respond, bool should_succeed = true) {
    auto_respond_ = auto_respond;
    should_succeed_ = should_succeed;
  }

  int create_output_stream_called_count() const {
    return create_output_stream_called_count_;
  }
  const std::string& last_device_id() const { return last_device_id_; }
  const media::AudioParameters& last_params() const { return last_params_; }
  const base::UnguessableToken& last_group_id() const { return last_group_id_; }

 private:
  std::string last_device_id_;
  media::AudioParameters last_params_;
  base::UnguessableToken last_group_id_;
  mojo::PendingReceiver<media::mojom::AudioOutputStream> last_stream_receiver_;
  CreateOutputStreamCallback last_callback_;
  int create_output_stream_called_count_ = 0;
  bool auto_respond_ = true;
  bool should_succeed_ = true;
  base::OnceClosure create_output_stream_callback_;
};

}  // namespace readaloud

#endif  // CHROME_BROWSER_READALOUD_FAKE_AUDIO_STREAM_FACTORY_H_
