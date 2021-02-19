// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_INCOMING_FRAMES_READER_H_
#define CHROME_BROWSER_NEARBY_SHARING_INCOMING_FRAMES_READER_H_

#include <map>
#include <vector>

#include "base/callback_forward.h"
#include "base/cancelable_callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chromeos/services/nearby/public/cpp/nearby_process_manager.h"
#include "chromeos/services/nearby/public/mojom/nearby_decoder_types.mojom.h"

class NearbyConnection;

// Helper class to read incoming frames from Nearby devices.
class IncomingFramesReader {
 public:
  IncomingFramesReader(chromeos::nearby::NearbyProcessManager* process_manager,
                       NearbyConnection* connection);
  virtual ~IncomingFramesReader();
  IncomingFramesReader(const IncomingFramesReader&) = delete;
  IncomingFramesReader& operator=(IncomingFramesReader&) = delete;

  // Reads an incoming frame from |connection|. |callback| is called
  // with the frame read from connection or nullopt if connection socket is
  // closed.
  //
  // Note: Callers are expected wait for |callback| to be run before scheduling
  // subsequent calls to ReadFrame(..).
  virtual void ReadFrame(
      base::OnceCallback<void(base::Optional<sharing::mojom::V1FramePtr>)>
          callback);

  // Reads a frame of type |frame_type| from |connection|. |callback| is called
  // with the frame read from connection or nullopt if connection socket is
  // closed or |timeout| units of time has passed.
  //
  // Note: Callers are expected wait for |callback| to be run before scheduling
  // subsequent calls to ReadFrame(..).
  virtual void ReadFrame(
      sharing::mojom::V1Frame::Tag frame_type,
      base::OnceCallback<void(base::Optional<sharing::mojom::V1FramePtr>)>
          callback,
      base::TimeDelta timeout);

 private:
  void ReadNextFrame();
  void OnDataReadFromConnection(base::Optional<std::vector<uint8_t>> bytes);
  void OnFrameDecoded(sharing::mojom::FramePtr mojo_frame);
  void OnTimeout();
  void OnNearbyProcessStopped(
      chromeos::nearby::NearbyProcessManager::NearbyProcessShutdownReason
          shutdown_reason);
  void Done(base::Optional<sharing::mojom::V1FramePtr> frame);
  base::Optional<sharing::mojom::V1FramePtr> GetCachedFrame(
      base::Optional<sharing::mojom::V1Frame::Tag> frame_type);
  sharing::mojom::NearbySharingDecoder* GetOrStartNearbySharingDecoder();

  chromeos::nearby::NearbyProcessManager* process_manager_;
  std::unique_ptr<
      chromeos::nearby::NearbyProcessManager::NearbyProcessReference>
      process_reference_;
  NearbyConnection* connection_;
  base::Optional<sharing::mojom::V1Frame::Tag> frame_type_;
  base::OnceCallback<void(base::Optional<sharing::mojom::V1FramePtr>)>
      callback_;
  base::CancelableOnceClosure timeout_callback_;

  // Caches frames read from NearbyConnection which are not used immediately.
  std::map<sharing::mojom::V1Frame::Tag, sharing::mojom::V1FramePtr>
      cached_frames_;

  bool is_process_stopped_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IncomingFramesReader> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_INCOMING_FRAMES_READER_H_
