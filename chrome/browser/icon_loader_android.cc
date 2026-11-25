// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#include <string>
#include <utility>

#include "base/android/content_uri_utils.h"
#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/download/android/download_utils.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "components/offline_items_collection/core/offline_item_filter.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/mime_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/download/android/jni_headers/DownloadIconUtils_jni.h"

namespace {

// The largest icon that ReadIcon() renders, as a width and height in pixels.
// Icons are at most 48 DIPs, so this allows any real device scale factor.
constexpr int kMaxIconSizePixels = 1024;

// Returns the width and height in DIPs of an icon of `size`.
int GetIconSizeDip(IconLoader::IconSize size) {
  switch (size) {
    case IconLoader::SMALL:
      return 16;
    case IconLoader::NORMAL:
      return 32;
    case IconLoader::LARGE:
      return 48;
    case IconLoader::ALL:
      break;
  }
  NOTREACHED();
}

}  // namespace

// static
IconLoader::IconGroup IconLoader::GroupForFilepath(
    const base::FilePath& file_path) {
  // Like Linux, all files with the same MIME type share an icon. It is left
  // empty for an unknown type, which gets the generic file icon.
  if (file_path.IsContentUri()) {
    // A content URI, e.g. of a finished download, has no file extension, so
    // ask its content provider for the type.
    std::string mime_type = base::GetContentUriMimeType(file_path);
    // A type with no icon of its own may be a generic one such as
    // application/octet-stream. RemapGenericMimeType() replaces a generic type
    // with the type for the display name's extension, as the download system
    // does. This is skipped for other types because getting the display name
    // is a second query to the provider.
    const auto filter =
        OfflineItemUtils::MimeTypeToOfflineItemFilter(mime_type);
    if (filter == offline_items_collection::FILTER_OTHER ||
        filter == offline_items_collection::FILTER_DOCUMENT) {
      std::u16string display_name;
      if (base::MaybeGetFileDisplayName(file_path, &display_name)) {
        mime_type = DownloadUtils::RemapGenericMimeType(
            mime_type, GURL(), base::UTF16ToUTF8(display_name));
      }
    }
    return mime_type;
  }
  // The MIME type is derived from the file extension, so the file does not
  // need to exist.
  std::string mime_type;
  net::GetMimeTypeFromFile(file_path, &mime_type);
  return mime_type;
}

// static
scoped_refptr<base::TaskRunner> IconLoader::GetReadIconTaskRunner() {
  // ReadIcon() renders an Android drawable, so it must be on the UI thread.
  return content::GetUIThreadTaskRunner({});
}

void IconLoader::ReadIcon() {
  // Android has no OS-provided file type icons that are accessible from here,
  // so use the icons that Chrome shows for downloads of this type.
  gfx::Image image;
  // Java can't create a bitmap with no pixels, and runs out of memory for a
  // huge one, so fail instead for a scale that gives either. A NaN scale fails
  // both comparisons.
  const float size_pixels = GetIconSizeDip(icon_size_) * scale_;
  if (size_pixels >= 1.f && size_pixels <= kMaxIconSizePixels) {
    const SkBitmap bitmap = DownloadIconUtilsJni::getFileTypeIcon(
        base::android::AttachCurrentThread(),
        OfflineItemUtils::MimeTypeToOfflineItemFilter(group_),
        base::ClampCeil(size_pixels));
    image = gfx::Image(gfx::ImageSkia::CreateFromBitmap(bitmap, scale_));
  }

  target_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), std::move(image), group_));
  delete this;
}
