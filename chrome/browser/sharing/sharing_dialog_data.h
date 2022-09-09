// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_DIALOG_DATA_H_
#define CHROME_BROWSER_SHARING_SHARING_DIALOG_DATA_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/sharing/sharing_app.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "components/sync_device_info/device_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

class SharingDialog;

namespace gfx {
struct VectorIcon;
}  // namespace gfx

// All data required to display a SharingDialog.
struct SharingDialogData {
 public:
  // TODO(crbug.com/1013099): Merge both images using alpha blending so they
  // work on any background color.
  struct HeaderIcons {
    HeaderIcons(const gfx::VectorIcon* light, const gfx::VectorIcon* dark);
    const gfx::VectorIcon* light;
    const gfx::VectorIcon* dark;
  };
  SharingDialogData();
  ~SharingDialogData();
  SharingDialogData(SharingDialogData&& other);
  SharingDialogData& operator=(SharingDialogData&& other);

  SharingDialogType type = SharingDialogType::kErrorDialog;
  SharingFeatureName prefix = SharingFeatureName::kUnknown;

  std::vector<std::unique_ptr<syncer::DeviceInfo>> devices;
  std::vector<SharingApp> apps;

  std::u16string title;
  std::u16string error_text;
  int help_text_id = 0;
  int help_text_origin_id = 0;
  absl::optional<HeaderIcons> header_icons;
  int origin_text_id = 0;
  absl::optional<url::Origin> initiating_origin;

  base::OnceCallback<void(const syncer::DeviceInfo&)> device_callback;
  base::OnceCallback<void(const SharingApp&)> app_callback;
  base::OnceCallback<void(SharingDialog*)> close_callback;
};

#endif  // CHROME_BROWSER_SHARING_SHARING_DIALOG_DATA_H_
