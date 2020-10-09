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
#include "chrome/browser/nearby_sharing/nearby_process_manager.h"
#include "chromeos/services/nearby/public/mojom/nearby_decoder_types.mojom.h"

class NearbyConnection;
class Profile;

// Helper class to read incoming frames from Nearby devices.
class IncomingFramesReader : public NearbyProcessManager::Observer {
 public:
  IncomingFramesReader(NearbyProcessManager* process_manager,
                       Profile* profile,
                       NearbyConnection* connection);
  ~IncomingFramesReader() override;

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
  // NearbyProcessManager::Observer:
  void OnNearbyProfileChanged(Profile* profile) override;
  void OnNearbyProcessStarted() override;
  void OnNearbyProcessStopped() override;

  void ReadNextFrame();
  void OnDataReadFromConnection(base::Optional<std::vector<uint8_t>> bytes);
  void OnFrameDecoded(sharing::mojom::FramePtr mojo_frame);
  void OnTimeout();
  void Done(base::Optional<sharing::mojom::V1FramePtr> frame);
  base::Optional<sharing::mojom::V1FramePtr> GetCachedFrame(
      base::Optional<sharing::mojom::V1Frame::Tag> frame_type);

  NearbyProcessManager* process_manager_;
  Profile* profile_;
  NearbyConnection* connection_;
  base::Optional<sharing::mojom::V1Frame::Tag> frame_type_;
  base::OnceCallback<void(base::Optional<sharing::mojom::V1FramePtr>)>
      callback_;
  base::CancelableOnceClosure timeout_callback_;

  // Caches frames read from NearbyConnection which are not used immediately.
  std::map<sharing::mojom::V1Frame::Tag, sharing::mojom::V1FramePtr>
      cached_frames_;

  bool is_process_stopped_ = false;
  ScopedObserver<NearbyProcessManager, NearbyProcessManager::Observer>
      nearby_process_observer_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IncomingFramesReader> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_INCOMING_FRAMES_READER_H_
