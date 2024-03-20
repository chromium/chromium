// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAINT_PREVIEW_SERVICES_PAINT_PREVIEW_TAB_SERVICE_FILE_MIXIN_H_
#define CHROME_BROWSER_PAINT_PREVIEW_SERVICES_PAINT_PREVIEW_TAB_SERVICE_FILE_MIXIN_H_

#include <string_view>

#include "components/paint_preview/browser/paint_preview_file_mixin.h"

namespace paint_preview {

class PaintPreviewTabServiceFileMixin : public PaintPreviewFileMixin {
 public:
  PaintPreviewTabServiceFileMixin(const base::FilePath& profile_dir,
                                  std::string_view ascii_feature_name);
  PaintPreviewTabServiceFileMixin(const PaintPreviewTabServiceFileMixin&) =
      delete;
  PaintPreviewTabServiceFileMixin& operator=(
      const PaintPreviewTabServiceFileMixin&) = delete;
  ~PaintPreviewTabServiceFileMixin() override;

  // Override for GetCapturedPaintPreviewProto. Defaults expiry horizon to 72
  // hrs if not specified.
  void GetCapturedPaintPreviewProto(
      const DirectoryKey& key,
      std::optional<base::TimeDelta> expiry_horizon,
      OnReadProtoCallback on_read_proto_callback) override;

  // The time horizon after which unused paint previews will be deleted.
  static constexpr int kExpiryHorizonHrs = 72;
};

}  // namespace paint_preview

#endif  // CHROME_BROWSER_PAINT_PREVIEW_SERVICES_PAINT_PREVIEW_TAB_SERVICE_FILE_MIXIN_H_
