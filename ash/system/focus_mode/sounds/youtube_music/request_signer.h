// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_YOUTUBE_MUSIC_REQUEST_SIGNER_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_YOUTUBE_MUSIC_REQUEST_SIGNER_H_

#include "ash/ash_export.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "components/account_id/account_id.h"

namespace ash {

// Retrieves an X.509 certificate and generates headers appropriate for signing
// YMC API requets with that certificate.
class ASH_EXPORT RequestSigner {
 public:
  virtual ~RequestSigner() = default;

  // Adds the appropriate headers to attest that the request containing `data`
  // is from a device with a client certificate.
  using HeadersCallback =
      base::OnceCallback<void(const std::vector<std::string>& headers)>;
  virtual bool GenerateHeaders(base::span<const uint8_t> data,
                               HeadersCallback callback) = 0;

  // Returns a completed 'Device-Info' header for requests that don't require
  // signing.
  virtual std::string DeviceInfoHeader() = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_YOUTUBE_MUSIC_REQUEST_SIGNER_H_
