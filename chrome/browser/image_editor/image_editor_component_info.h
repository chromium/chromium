// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMAGE_EDITOR_IMAGE_EDITOR_COMPONENT_INFO_H_
#define CHROME_BROWSER_IMAGE_EDITOR_IMAGE_EDITOR_COMPONENT_INFO_H_

#include "base/files/file_path.h"
#include "base/memory/singleton.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace image_editor {

// Contains information about installed image editor component.
class ImageEditorComponentInfo {
 public:
  static ImageEditorComponentInfo* GetInstance();
  ImageEditorComponentInfo(const ImageEditorComponentInfo&) = delete;
  ImageEditorComponentInfo& operator=(const ImageEditorComponentInfo&) = delete;

  // Sets the component installed path for use in the UI loader.
  void SetInstalledPath(const base::FilePath& path);

  // Obtains the component installed path, or an empty path.
  base::FilePath GetInstalledPath() const;

  // Returns whether the image editor is available and can be used.
  bool IsImageEditorAvailable() const;

 private:
  ImageEditorComponentInfo();
  friend struct base::DefaultSingletonTraits<ImageEditorComponentInfo>;

  // Installed component path, or empty.
  base::FilePath installed_path_;
};
}  // namespace image_editor

#endif  // CHROME_BROWSER_IMAGE_EDITOR_IMAGE_EDITOR_COMPONENT_INFO_H_
