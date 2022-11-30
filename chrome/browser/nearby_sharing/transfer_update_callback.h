// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_TRANSFER_UPDATE_CALLBACK_H_
#define CHROME_BROWSER_NEARBY_SHARING_TRANSFER_UPDATE_CALLBACK_H_

#include "base/observer_list_types.h"
#include "chrome/browser/nearby_sharing/share_target.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"

// Reports the transfer status for an ongoing transfer with a |share_target|.
class TransferUpdateCallback : public base::CheckedObserver {
 public:
  virtual void OnTransferUpdate(const ShareTarget& share_target,
                                const TransferMetadata& transfer_metadata) = 0;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_TRANSFER_UPDATE_CALLBACK_H_
