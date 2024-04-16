// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/image_editor/image_editor_component_info.h"

#include "base/files/file_path.h"
#include "base/memory/singleton.h"

namespace image_editor {

ImageEditorComponentInfo::ImageEditorComponentInfo() = default;

ImageEditorComponentInfo* ImageEditorComponentInfo::GetInstance() {
  return base::Singleton<ImageEditorComponentInfo>::get();
}

void ImageEditorComponentInfo::SetInstalledPath(const base::FilePath& path) {
  installed_path_ = path;
}

base::FilePath ImageEditorComponentInfo::GetInstalledPath() const {
  return installed_path_;
}

bool ImageEditorComponentInfo::IsImageEditorAvailable() const {
  // Currently this feature is available unconditionally if the component is
  // installed.
  // TODO(crbug.com/40222495): Differentiate on multiple features.
  return !installed_path_.empty();
}

}  // namespace image_editor
