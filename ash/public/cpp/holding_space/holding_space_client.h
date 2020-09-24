// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CLIENT_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CLIENT_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash {

class HoldingSpaceItem;

// Interface for the holding space browser client.
class ASH_PUBLIC_EXPORT HoldingSpaceClient {
 public:
  using SuccessCallback = base::OnceCallback<void(bool)>;

  // Adds a screenshot item backed by the provided `file_path`.
  virtual void AddScreenshot(const base::FilePath& file_path) = 0;

  // Attempts to copy the contents of the image file backing the specified
  // holding space `item` to the clipboard. If the backing file is not suspected
  // to contain image data, this method will abort early. Success is returned
  // via the supplied `callback`.
  virtual void CopyImageToClipboard(const HoldingSpaceItem& item,
                                    SuccessCallback callback) = 0;

  // Attempts to open the Downloads folder.
  // Success is returned via the supplied `callback`.
  virtual void OpenDownloads(SuccessCallback callback) = 0;

  // Attempts to open the specified holding space `items`.
  // Success is returned via the supplied `callback`.
  virtual void OpenItems(const std::vector<const HoldingSpaceItem*>& items,
                         SuccessCallback callback) = 0;

  // Attempts to show the specified holding space `item` in its folder.
  // Success is returned via the supplied `callback`.
  virtual void ShowItemInFolder(const HoldingSpaceItem& item,
                                SuccessCallback callback) = 0;

  // Pins the specified holding space `items`.
  virtual void PinItems(const std::vector<const HoldingSpaceItem*>& items) = 0;

  // Unpins the specified holding space `items`.
  virtual void UnpinItems(
      const std::vector<const HoldingSpaceItem*>& items) = 0;

 protected:
  HoldingSpaceClient() = default;
  virtual ~HoldingSpaceClient() = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CLIENT_H_
