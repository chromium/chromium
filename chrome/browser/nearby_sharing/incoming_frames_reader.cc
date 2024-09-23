// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/incoming_frames_reader.h"

#include <type_traits>

#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connection.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_decoder.mojom.h"
#include "components/cross_device/logging/logging.h"

namespace {

std::ostream& operator<<(std::ostream& out,
                         const sharing::mojom::V1Frame::Tag& obj) {
  out << static_cast<std::underlying_type<sharing::mojom::V1Frame::Tag>::type>(
      obj);
  return out;
}

}  // namespace

IncomingFramesReader::IncomingFramesReader(
    ash::nearby::NearbyProcessManager* process_manager,
    NearbyConnection* connection)
    : process_manager_(process_manager), connection_(connection) {
  DCHECK(process_manager);
  DCHECK(connection);
}

IncomingFramesReader::~IncomingFramesReader() = default;

void IncomingFramesReader::ReadFrame(
    base::OnceCallback<void(std::optional<sharing::mojom::V1FramePtr>)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback_);
  DCHECK(!is_process_stopped_);

  callback_ = std::move(callback);
  frame_type_ = std::nullopt;

  // Check in cache for frame.
  std::optional<sharing::mojom::V1FramePtr> cached_frame =
      GetCachedFrame(frame_type_);
  if (cached_frame) {
    Done(std::move(cached_frame));
    return;
  }

  ReadNextFrame();
}

void IncomingFramesReader::ReadFrame(
    sharing::mojom::V1Frame::Tag frame_type,
    base::OnceCallback<void(std::optional<sharing::mojom::V1FramePtr>)>
        callback,
    base::TimeDelta timeout) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback_);
  DCHECK(!is_process_stopped_);
  if (!connection_) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  callback_ = std::move(callback);
  frame_type_ = frame_type;

  timeout_callback_.Reset(base::BindOnce(&IncomingFramesReader::OnTimeout,
                                         weak_ptr_factory_.GetWeakPtr()));
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(timeout_callback_.callback()), timeout);

  // Check in cache for frame.
  std::optional<sharing::mojom::V1FramePtr> cached_frame =
      GetCachedFrame(frame_type_);
  if (cached_frame) {
    Done(std::move(cached_frame));
    return;
  }

  ReadNextFrame();
}

void IncomingFramesReader::OnNearbyProcessStopped(
    ash::nearby::NearbyProcessManager::NearbyProcessShutdownReason) {
  is_process_stopped_ = true;
  Done(std::nullopt);
}

void IncomingFramesReader::ReadNextFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connection_->Read(
      base::BindOnce(&IncomingFramesReader::OnDataReadFromConnection,
                     weak_ptr_factory_.GetWeakPtr()));
}

void IncomingFramesReader::OnTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CD_LOG(WARNING, Feature::NS)
      << __func__ << ": Timed out reading from NearbyConnection.";
  Done(std::nullopt);
}

void IncomingFramesReader::OnDataReadFromConnection(
    std::optional<std::vector<uint8_t>> bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!callback_) {
    return;
  }

  if (!bytes) {
    CD_LOG(WARNING, Feature::NS) << __func__ << ": Failed to read frame";
    Done(std::nullopt);
    return;
  }

  sharing::mojom::NearbySharingDecoder* decoder =
      GetOrStartNearbySharingDecoder();

  if (!decoder) {
    CD_LOG(WARNING, Feature::NS)
        << __func__
        << ": Cannot decode frame. Not currently bound to nearby process";
    Done(std::nullopt);
    return;
  }

  decoder->DecodeFrame(*bytes,
                       base::BindOnce(&IncomingFramesReader::OnFrameDecoded,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void IncomingFramesReader::OnFrameDecoded(sharing::mojom::FramePtr frame) {
  if (!frame) {
    ReadNextFrame();
    return;
  }

  if (!frame->is_v1()) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Frame read does not have V1Frame";
    ReadNextFrame();
    return;
  }

  sharing::mojom::V1FramePtr v1_frame(std::move(frame->get_v1()));
  sharing::mojom::V1Frame::Tag v1_frame_type = v1_frame->which();
  if (frame_type_ && *frame_type_ != v1_frame_type) {
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Failed to read frame of type " << *frame_type_
        << ", but got frame of type " << v1_frame_type << ". Cached for later.";
    cached_frames_.insert({v1_frame_type, std::move(v1_frame)});
    ReadNextFrame();
    return;
  }

  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Successfully read frame of type " << v1_frame_type;
  Done(std::move(v1_frame));
}

void IncomingFramesReader::Done(
    std::optional<sharing::mojom::V1FramePtr> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  frame_type_ = std::nullopt;
  timeout_callback_.Cancel();
  if (callback_) {
    std::move(callback_).Run(std::move(frame));
  }
}

std::optional<sharing::mojom::V1FramePtr> IncomingFramesReader::GetCachedFrame(
    std::optional<sharing::mojom::V1Frame::Tag> frame_type) {
  CD_LOG(VERBOSE, Feature::NS) << __func__ << ": Fetching cached frame";
  if (frame_type)
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Requested frame type - " << *frame_type;

  auto iter =
      frame_type ? cached_frames_.find(*frame_type) : cached_frames_.begin();

  if (iter == cached_frames_.end())
    return std::nullopt;

  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Successfully read cached frame";
  sharing::mojom::V1FramePtr frame = std::move(iter->second);
  cached_frames_.erase(iter);
  return frame;
}

sharing::mojom::NearbySharingDecoder*
IncomingFramesReader::GetOrStartNearbySharingDecoder() {
  if (!process_reference_) {
    process_reference_ = process_manager_->GetNearbyProcessReference(
        base::BindOnce(&IncomingFramesReader::OnNearbyProcessStopped,
                       weak_ptr_factory_.GetWeakPtr()));

    if (!process_reference_) {
      CD_LOG(WARNING, Feature::NS)
          << __func__ << "Failed to get a reference to the nearby process.";
      is_process_stopped_ = true;
      return nullptr;
    }
  }

  is_process_stopped_ = false;

  sharing::mojom::NearbySharingDecoder* decoder =
      process_reference_->GetNearbySharingDecoder().get();

  if (!decoder)
    CD_LOG(WARNING, Feature::NS)
        << __func__ << "Failed to get decoder from process reference.";

  return decoder;
}
