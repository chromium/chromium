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
  // Attempts to open the specified holding space `item`. Success is returned
  // via the supplied `callback`.
  using OpenItemCallback = base::OnceCallback<void(bool)>;
  virtual void OpenItem(const HoldingSpaceItem& item,
                        OpenItemCallback callback) = 0;

 protected:
  HoldingSpaceClient() = default;
  virtual ~HoldingSpaceClient() = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CLIENT_H_
