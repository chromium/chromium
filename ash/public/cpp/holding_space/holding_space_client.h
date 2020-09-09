// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CLIENT_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CLIENT_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"

namespace ash {

class HoldingSpaceItem;

// Interface for the holding space browser client.
class ASH_PUBLIC_EXPORT HoldingSpaceClient {
 public:
  using SuccessCallback = base::OnceCallback<void(bool)>;

  // Attempts to copy the specified holding space `item` to the clipboard.
  // Success is returned via the supplied `callback`.
  virtual void CopyToClipboard(const HoldingSpaceItem& item,
                               SuccessCallback callback) = 0;

  // Attempts to open the specified holding space `item`.
  // Success is returned via the supplied `callback`.
  virtual void OpenItem(const HoldingSpaceItem& item,
                        SuccessCallback callback) = 0;

  // Attempts to open the specified holding space `item` in its folder.
  // Success is returned via the supplied `callback`.
  virtual void OpenItemInFolder(const HoldingSpaceItem& item,
                                SuccessCallback callback) = 0;

  // Pins the specified `item`.
  virtual void PinItem(const HoldingSpaceItem& item) = 0;

  // Unpins the specified `item`.
  virtual void UnpinItem(const HoldingSpaceItem& item) = 0;

 protected:
  HoldingSpaceClient() = default;
  virtual ~HoldingSpaceClient() = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CLIENT_H_
