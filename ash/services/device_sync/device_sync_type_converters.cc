// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/device_sync/device_sync_type_converters.h"

namespace mojo {

// static
chromeos::device_sync::mojom::NetworkRequestResult
TypeConverter<chromeos::device_sync::mojom::NetworkRequestResult,
              chromeos::device_sync::NetworkRequestError>::
    Convert(chromeos::device_sync::NetworkRequestError type) {
  switch (type) {
    case chromeos::device_sync::NetworkRequestError::kOffline:
      return chromeos::device_sync::mojom::NetworkRequestResult::kOffline;
    case chromeos::device_sync::NetworkRequestError::kEndpointNotFound:
      return chromeos::device_sync::mojom::NetworkRequestResult::
          kEndpointNotFound;
    case chromeos::device_sync::NetworkRequestError::kAuthenticationError:
      return chromeos::device_sync::mojom::NetworkRequestResult::
          kAuthenticationError;
    case chromeos::device_sync::NetworkRequestError::kBadRequest:
      return chromeos::device_sync::mojom::NetworkRequestResult::kBadRequest;
    case chromeos::device_sync::NetworkRequestError::kResponseMalformed:
      return chromeos::device_sync::mojom::NetworkRequestResult::
          kResponseMalformed;
    case chromeos::device_sync::NetworkRequestError::kInternalServerError:
      return chromeos::device_sync::mojom::NetworkRequestResult::
          kInternalServerError;
    case chromeos::device_sync::NetworkRequestError::kUnknown:
      return chromeos::device_sync::mojom::NetworkRequestResult::kUnknown;
  }
}

}  // namespace mojo
