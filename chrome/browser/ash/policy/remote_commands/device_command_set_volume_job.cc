// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_set_volume_job.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/syslog_logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

namespace {

bool ShouldMute(const ash::CrasAudioHandler& audio_handler,
                int new_output_volume) {
  // Note that the new output volume only takes effect asynchronously,
  // so we can not use `CrasAudioHandler::IsOutputVolumeBelowDefaultMuteLevel()`
  // as it will return an answer based on the old output volume.
  return new_output_volume <
         audio_handler.GetOutputDefaultVolumeMuteThreshold();
}

}  // namespace

constexpr char DeviceCommandSetVolumeJob::kVolumeFieldName[] = "volume";

DeviceCommandSetVolumeJob::DeviceCommandSetVolumeJob() = default;

DeviceCommandSetVolumeJob::~DeviceCommandSetVolumeJob() = default;

enterprise_management::RemoteCommand_Type DeviceCommandSetVolumeJob::GetType()
    const {
  return enterprise_management::RemoteCommand_Type_DEVICE_SET_VOLUME;
}

bool DeviceCommandSetVolumeJob::ParseCommandPayload(
    const std::string& command_payload) {
  std::optional<base::Value> root(base::JSONReader::Read(command_payload));
  if (!root || !root->is_dict()) {
    return false;
  }
  std::optional<int> maybe_volume;
  maybe_volume = root->GetDict().FindInt(kVolumeFieldName);
  if (!maybe_volume) {
    return false;
  }
  volume_ = *maybe_volume;
  if (volume_ < 0 || volume_ > 100) {
    return false;
  }
  return true;
}

void DeviceCommandSetVolumeJob::RunImpl(CallbackWithResult result_callback) {
  SYSLOG(INFO) << "Running set volume command, volume = " << volume_;
  auto& audio_handler = CHECK_DEREF(ash::CrasAudioHandler::Get());

  audio_handler.SetOutputVolumePercent(volume_);
  audio_handler.SetOutputMute(ShouldMute(audio_handler, volume_));

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(result_callback),
                                ResultType::kSuccess, std::nullopt));
}

}  // namespace policy
