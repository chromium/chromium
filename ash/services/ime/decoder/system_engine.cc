// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/ime/decoder/system_engine.h"

#include "ash/services/ime/constants.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"

namespace ash {
namespace ime {

SystemEngine::SystemEngine(
    ImeCrosPlatform* platform,
    absl::optional<ImeDecoder::EntryPoints> entry_points) {
  if (!entry_points) {
    LOG(WARNING) << "SystemEngine INIT INCOMPLETE.";
    return;
  }

  decoder_entry_points_ = *entry_points;
  decoder_entry_points_->init_mojo_mode(platform);
}

SystemEngine::~SystemEngine() {
  if (!decoder_entry_points_) {
    return;
  }

  decoder_entry_points_->close_mojo_mode();
}

bool SystemEngine::BindRequest(
    const std::string& ime_spec,
    mojo::PendingReceiver<mojom::InputMethod> receiver,
    mojo::PendingRemote<mojom::InputMethodHost> host) {
  if (!decoder_entry_points_) {
    return false;
  }

  auto receiver_pipe_handle = receiver.PassPipe().release().value();
  auto host_pipe_version = host.version();
  auto host_pipe_handle = host.PassPipe().release().value();
  return decoder_entry_points_->connect_to_input_method(
      ime_spec.c_str(), receiver_pipe_handle, host_pipe_handle,
      host_pipe_version);
}

bool SystemEngine::BindConnectionFactory(
    mojo::PendingReceiver<mojom::ConnectionFactory> receiver) {
  if (!decoder_entry_points_)
    return false;
  auto receiver_pipe_handle = receiver.PassPipe().release().value();
  return decoder_entry_points_->initialize_connection_factory(
      receiver_pipe_handle);
}

bool SystemEngine::IsConnected() {
  return decoder_entry_points_ &&
         decoder_entry_points_->is_input_method_connected();
}

}  // namespace ime
}  // namespace ash
