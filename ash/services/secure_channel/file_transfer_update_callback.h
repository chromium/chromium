// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_FILE_TRANSFER_UPDATE_CALLBACK_H_
#define ASH_SERVICES_SECURE_CHANNEL_FILE_TRANSFER_UPDATE_CALLBACK_H_

#include "ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "base/callback.h"

namespace chromeos {

namespace secure_channel {

// Syntactic sugar to make it easier to declare callbacks to the
// RegisterPayloadFile APIs.
using FileTransferUpdateCallback =
    base::RepeatingCallback<void(mojom::FileTransferUpdatePtr)>;

}  // namespace secure_channel

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace ash::secure_channel {
using ::chromeos::secure_channel::FileTransferUpdateCallback;
}

#endif  // ASH_SERVICES_SECURE_CHANNEL_FILE_TRANSFER_UPDATE_CALLBACK_H_
