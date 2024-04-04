// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/lens_overlay/lens_overlay_request_id_generator.h"

#include "base/rand_util.h"
#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"

namespace lens {

LensOverlayRequestIdGenerator::LensOverlayRequestIdGenerator() {
  LensOverlayRequestIdGenerator::ResetRequestId();
}

LensOverlayRequestIdGenerator::~LensOverlayRequestIdGenerator() = default;

void LensOverlayRequestIdGenerator::ResetRequestId() {
  uuid_ = base::RandUint64();
  sequence_id_ = 1;
}

std::unique_ptr<lens::LensOverlayRequestId>
LensOverlayRequestIdGenerator::GetNextRequestId() {
  auto request_id = std::make_unique<lens::LensOverlayRequestId>();
  request_id->set_uuid(uuid_);
  request_id->set_sequence_id(sequence_id_);
  // The server expects the image sequence id to be set, even though
  // it will never change.
  request_id->set_image_sequence_id(1);

  // Increment the sequence id for the next request.
  sequence_id_++;
  return request_id;
}

}  // namespace lens
