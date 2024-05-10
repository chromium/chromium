// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/shortcut_creator.h"

#include <string>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "ui/gfx/image/image_family.h"
#include "url/gurl.h"

namespace shortcuts {

ShortcutMetadata::ShortcutMetadata() = default;
ShortcutMetadata::ShortcutMetadata(base::FilePath profile_path,
                                   GURL shortcut_url,
                                   std::u16string shortcut_title,
                                   gfx::ImageFamily shortcut_images)
    : profile_path(profile_path),
      shortcut_url(shortcut_url),
      shortcut_title(shortcut_title),
      shortcut_images(std::move(shortcut_images)) {
  CHECK(IsValid());
}
ShortcutMetadata::~ShortcutMetadata() = default;

ShortcutMetadata::ShortcutMetadata(ShortcutMetadata&& metadata) = default;
ShortcutMetadata& ShortcutMetadata::operator=(ShortcutMetadata&& metadata) =
    default;

bool ShortcutMetadata::IsValid() {
  return !profile_path.empty() && shortcut_url.is_valid() &&
         !shortcut_title.empty() && profile_path.BaseName() != profile_path &&
         !shortcut_images.empty();
}

void EmitIconStorageCountMetric(const base::FilePath& icon_directory) {
  size_t num_files = 0;
  base::FileEnumerator file_iter(icon_directory, /*recursive=*/false,
                                 base::FileEnumerator::FILES);
  while (!file_iter.Next().empty()) {
    ++num_files;
  }
  // Note: If this metric shows that the icon directory is getting too full,
  // then a new design or cleanup may be required.
  base::UmaHistogramCounts100000("Shortcuts.Icons.StorageCount", num_files);
}

}  // namespace shortcuts
