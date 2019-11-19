// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_DIALOG_DATA_H_
#define CHROME_BROWSER_SHARING_SHARING_DIALOG_DATA_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "chrome/browser/sharing/sharing_app.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "components/sync_device_info/device_info.h"
#include "url/origin.h"

class SharingDialog;

namespace gfx {
struct VectorIcon;
}  // namespace gfx

// All data required to display a SharingDialog.
struct SharingDialogData {
 public:
  SharingDialogData();
  ~SharingDialogData();
  SharingDialogData(SharingDialogData&& other);
  SharingDialogData& operator=(SharingDialogData&& other);

  SharingDialogType type = SharingDialogType::kErrorDialog;
  SharingFeatureName prefix = SharingFeatureName::kUnknown;

  std::vector<std::unique_ptr<syncer::DeviceInfo>> devices;
  std::vector<SharingApp> apps;

  base::string16 title;
  base::string16 error_text;
  int help_text_id = 0;
  int help_text_origin_id = 0;
  int help_link_text_id = 0;
  const gfx::VectorIcon* header_image_light = nullptr;
  const gfx::VectorIcon* header_image_dark = nullptr;
  int origin_text_id = 0;
  base::Optional<url::Origin> initiating_origin;

  base::OnceCallback<void(SharingDialogType)> help_callback;
  base::OnceCallback<void(const syncer::DeviceInfo&)> device_callback;
  base::OnceCallback<void(const SharingApp&)> app_callback;
  base::OnceCallback<void(SharingDialog*)> close_callback;
};

#endif  // CHROME_BROWSER_SHARING_SHARING_DIALOG_DATA_H_
