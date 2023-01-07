// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#include "base/strings/string_util.h"
#include "chromeos/ui/base/file_icon_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

// static
IconLoader::IconGroup IconLoader::GroupForFilepath(
    const base::FilePath& file_path) {
  return base::ToLowerASCII(file_path.Extension());
}

// The Chrome OS implementation doesn't use an I/O thread because vector icons
// are created on the UI thread.
void IconLoader::Start() {
  int dip_size = 0;
  switch (icon_size_) {
    case IconLoader::SMALL:
      dip_size = 16;
      break;
    case IconLoader::NORMAL:
      dip_size = 32;
      break;
    case IconLoader::LARGE:
    case IconLoader::ALL:
      dip_size = 48;
      break;
  }
  std::move(callback_).Run(
      gfx::Image(chromeos::GetIconForPath(file_path_, /*dark_background=*/false,
                                          dip_size)),
      GroupForFilepath(file_path_));
  delete this;
}
