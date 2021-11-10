// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/ime/decoder/system_engine.h"

#include "ash/services/ime/constants.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"

namespace chromeos {
namespace ime {

SystemEngine::SystemEngine(ImeCrosPlatform* platform) : platform_(platform) {
  if (!TryLoadDecoder()) {
    LOG(WARNING) << "DecoderEngine INIT INCOMPLETED.";
  }
}

SystemEngine::~SystemEngine() {}

bool SystemEngine::TryLoadDecoder() {
  auto* decoder = ImeDecoder::GetInstance();
  if (decoder->GetStatus() == ImeDecoder::Status::kSuccess &&
      decoder->GetEntryPoints().is_ready) {
    decoder_entry_points_ = decoder->GetEntryPoints();
    decoder_entry_points_->init_once(platform_);
    return true;
  }
  return false;
}

bool SystemEngine::BindRequest(
    const std::string& ime_spec,
    mojo::PendingReceiver<mojom::InputMethod> receiver,
    mojo::PendingRemote<mojom::InputMethodHost> host) {
  auto receiver_pipe_handle = receiver.PassPipe().release().value();
  auto host_pipe_version = host.version();
  auto host_pipe_handle = host.PassPipe().release().value();
  return decoder_entry_points_->connect_to_input_method(
      ime_spec.c_str(), receiver_pipe_handle, host_pipe_handle,
      host_pipe_version);
}

bool SystemEngine::IsConnected() {
  return decoder_entry_points_->is_input_method_connected();
}

}  // namespace ime
}  // namespace chromeos
