// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/file_system_watcher/arc_file_system_watcher_util.h"

#include "base/logging.h"
#include "base/strings/string_util.h"

namespace arc {

// The set of media file extensions supported by Android MediaScanner.
// Entries must be lower-case and sorted by lexicographical order for
// binary search.
//
// The current list was taken from aosp-nougat version of
// frameworks/base/media/java/android/media/MediaFile.java, and included
// file type categories are:
// - Audio
// - MIDI
// - Video
// - Image
// - Raw image
const char* kAndroidSupportedMediaExtensions[] = {
    ".3g2",    // FILE_TYPE_3GPP2, video/3gpp2
    ".3gp",    // FILE_TYPE_3GPP, video/3gpp
    ".3gpp",   // FILE_TYPE_3GPP, video/3gpp
    ".3gpp2",  // FILE_TYPE_3GPP2, video/3gpp2
    ".aac",    // FILE_TYPE_AAC, audio/aac, audio/aac-adts
    ".amr",    // FILE_TYPE_AMR, audio/amr
    ".arw",    // FILE_TYPE_ARW, image/x-sony-arw
    ".asf",    // FILE_TYPE_ASF, video/x-ms-asf
    ".avi",    // FILE_TYPE_AVI, video/avi
    ".awb",    // FILE_TYPE_AWB, audio/amr-wb
    ".bmp",    // FILE_TYPE_BMP, image/x-ms-bmp
    ".cr2",    // FILE_TYPE_CR2, image/x-canon-cr2
    ".dng",    // FILE_TYPE_DNG, image/x-adobe-dng
    ".fl",     // FILE_TYPE_FL, application/x-android-drm-fl
    ".gif",    // FILE_TYPE_GIF, image/gif
    ".imy",    // FILE_TYPE_IMY, audio/imelody
    ".jpeg",   // FILE_TYPE_JPEG, image/jpeg
    ".jpg",    // FILE_TYPE_JPEG, image/jpeg
    ".m4a",    // FILE_TYPE_M4A, audio/mp4
    ".m4v",    // FILE_TYPE_M4V, video/mp4
    ".mid",    // FILE_TYPE_MID, audio/midi
    ".midi",   // FILE_TYPE_MID, audio/midi
    ".mka",    // FILE_TYPE_MKA, audio/x-matroska
    ".mkv",    // FILE_TYPE_MKV, video/x-matroska
    ".mov",    // FILE_TYPE_QT, video/quicktime
    ".mp3",    // FILE_TYPE_MP3, audio/mpeg
    ".mp4",    // FILE_TYPE_MP4, video/mp4
    ".mpeg",   // FILE_TYPE_MP4, video/mpeg, video/mp2p
    ".mpg",    // FILE_TYPE_MP4, video/mpeg, video/mp2p
    ".mpga",   // FILE_TYPE_MP3, audio/mpeg
    ".mxmf",   // FILE_TYPE_MID, audio/midi
    ".nef",    // FILE_TYPE_NEF, image/x-nikon-nef
    ".nrw",    // FILE_TYPE_NRW, image/x-nikon-nrw
    ".oga",    // FILE_TYPE_OGG, application/ogg
    ".ogg",    // FILE_TYPE_OGG, audio/ogg, application/ogg
    ".orf",    // FILE_TYPE_ORF, image/x-olympus-orf
    ".ota",    // FILE_TYPE_MID, audio/midi
    ".pef",    // FILE_TYPE_PEF, image/x-pentax-pef
    ".png",    // FILE_TYPE_PNG, image/png
    ".raf",    // FILE_TYPE_RAF, image/x-fuji-raf
    ".rtttl",  // FILE_TYPE_MID, audio/midi
    ".rtx",    // FILE_TYPE_MID, audio/midi
    ".rw2",    // FILE_TYPE_RW2, image/x-panasonic-rw2
    ".smf",    // FILE_TYPE_SMF, audio/sp-midi
    ".srw",    // FILE_TYPE_SRW, image/x-samsung-srw
    ".ts",     // FILE_TYPE_MP2TS, video/mp2ts
    ".wav",    // FILE_TYPE_WAV, audio/x-wav
    ".wbmp",   // FILE_TYPE_WBMP, image/vnd.wap.wbmp
    ".webm",   // FILE_TYPE_WEBM, video/webm
    ".webp",   // FILE_TYPE_WEBP, image/webp
    ".wma",    // FILE_TYPE_WMA, audio/x-ms-wma
    ".wmv",    // FILE_TYPE_WMV, video/x-ms-wmv
    ".xmf",    // FILE_TYPE_MID, audio/midi
};
const int kAndroidSupportedMediaExtensionsSize =
    std::size(kAndroidSupportedMediaExtensions);

bool AppendRelativePathForRemovableMedia(const base::FilePath& cros_path,
                                         base::FilePath* android_path) {
  std::vector<base::FilePath::StringType> parent_components =
      base::FilePath(kCrosRemovableMediaDir).GetComponents();
  std::vector<base::FilePath::StringType> child_components =
      cros_path.GetComponents();
  auto child_itr = child_components.begin();
  for (const auto& parent_component : parent_components) {
    if (child_itr == child_components.end() || parent_component != *child_itr) {
      LOG(WARNING) << "|cros_path| is not under kCrosRemovableMediaDir.";
      return false;
    }
    ++child_itr;
  }
  if (child_itr == child_components.end()) {
    LOG(WARNING) << "The CrOS path doesn't have a component for device label.";
    return false;
  }
  // The device label (e.g. "UNTITLED" for /media/removable/UNTITLED/foo.jpg)
  // should be converted to removable_UNTITLED, since the prefix "removable_"
  // is appended to paths for removable media in Android.
  *android_path = android_path->Append(kRemovableMediaLabelPrefix + *child_itr);
  ++child_itr;
  for (; child_itr != child_components.end(); ++child_itr) {
    *android_path = android_path->Append(*child_itr);
  }
  return true;
}

base::FilePath GetAndroidPath(const base::FilePath& cros_path,
                              const base::FilePath& cros_dir,
                              const base::FilePath& android_dir) {
  base::FilePath android_path(android_dir);
  if (cros_dir.value() == kCrosRemovableMediaDir) {
    if (!AppendRelativePathForRemovableMedia(cros_path, &android_path))
      return base::FilePath();
  } else {
    cros_dir.AppendRelativePath(cros_path, &android_path);
  }
  return android_path;
}

bool HasAndroidSupportedMediaExtension(const base::FilePath& path) {
  const std::string extension = base::ToLowerASCII(path.Extension());
  const auto less_comparator = [](const char* a, const char* b) {
    return strcmp(a, b) < 0;
  };
  DCHECK(std::is_sorted(
      kAndroidSupportedMediaExtensions,
      kAndroidSupportedMediaExtensions + kAndroidSupportedMediaExtensionsSize,
      less_comparator));
  return std::binary_search(
      kAndroidSupportedMediaExtensions,
      kAndroidSupportedMediaExtensions + kAndroidSupportedMediaExtensionsSize,
      extension.c_str(), less_comparator);
}

}  // namespace arc
