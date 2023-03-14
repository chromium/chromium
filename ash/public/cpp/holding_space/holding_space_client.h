// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CLIENT_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CLIENT_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/functional/callback_forward.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace ash {

// Interface for the holding space browser client.
class ASH_PUBLIC_EXPORT HoldingSpaceClient {
 public:
  using SuccessCallback = base::OnceCallback<void(bool)>;

  // Adds a diagnostics log item backed by the provided `file_path`.
  virtual void AddDiagnosticsLog(const base::FilePath& file_path) = 0;

  // Adds a screen capture item backed by the provided `file_path`.
  // NOTE: `type` must refer to a screen capture type.
  virtual void AddScreenCapture(HoldingSpaceItem::Type type,
                                const base::FilePath& file_path) = 0;

  // Attempts to copy the contents of the image file backing the specified
  // holding space `item` to the clipboard. If the backing file is not suspected
  // to contain image data, this method will abort early. Success is returned
  // via the supplied `callback`.
  virtual void CopyImageToClipboard(const HoldingSpaceItem& item,
                                    SuccessCallback callback) = 0;

  // Returns the file path from cracking the specified `file_system_url`.
  virtual base::FilePath CrackFileSystemUrl(
      const GURL& file_system_url) const = 0;

  // Returns the value of the `drive::prefs::kDisableDrive` pref, indicating
  // whether Google Drive has been disabled.
  virtual bool IsDriveDisabled() const = 0;

  // Attempts to open the Downloads folder.
  // Success is returned via the supplied `callback`.
  virtual void OpenDownloads(SuccessCallback callback) = 0;

  // Attempts to open the specified holding space `items`.
  // Success is returned via the supplied `callback`.
  virtual void OpenItems(const std::vector<const HoldingSpaceItem*>& items,
                         SuccessCallback callback) = 0;

  // Attempts to open the My Files folder.
  // Success is returned via the supplied `callback`.
  virtual void OpenMyFiles(SuccessCallback callback) = 0;

  // Pins the specified `file_paths`.
  virtual void PinFiles(const std::vector<base::FilePath>& file_paths) = 0;

  // Pins the specified holding space `items`.
  virtual void PinItems(const std::vector<const HoldingSpaceItem*>& items) = 0;

  // Remove file suggestions specified by absolute file paths.
  virtual void RemoveFileSuggestions(
      const std::vector<base::FilePath>& absolute_file_paths) = 0;

  // Attempts to show the specified holding space `item` in its folder.
  // Success is returned via the supplied `callback`.
  virtual void ShowItemInFolder(const HoldingSpaceItem& item,
                                SuccessCallback callback) = 0;

  // Unpins the specified holding space `items`.
  virtual void UnpinItems(
      const std::vector<const HoldingSpaceItem*>& items) = 0;

 protected:
  HoldingSpaceClient() = default;
  virtual ~HoldingSpaceClient() = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CLIENT_H_
