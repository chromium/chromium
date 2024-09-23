// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/paint_preview/services/paint_preview_tab_service_file_mixin.h"

#include <string_view>

#include "components/paint_preview/browser/paint_preview_file_mixin.h"

namespace paint_preview {

PaintPreviewTabServiceFileMixin::PaintPreviewTabServiceFileMixin(
    const base::FilePath& path,
    std::string_view ascii_feature_name)
    : PaintPreviewFileMixin(path, ascii_feature_name) {}

PaintPreviewTabServiceFileMixin::~PaintPreviewTabServiceFileMixin() = default;

void PaintPreviewTabServiceFileMixin::GetCapturedPaintPreviewProto(
    const DirectoryKey& key,
    std::optional<base::TimeDelta> expiry_horizon,
    OnReadProtoCallback on_read_proto_callback) {
  PaintPreviewFileMixin::GetCapturedPaintPreviewProto(
      key,
      expiry_horizon.has_value() ? expiry_horizon.value()
                                 : base::Hours(kExpiryHorizonHrs),
      std::move(on_read_proto_callback));
}

}  // namespace paint_preview
