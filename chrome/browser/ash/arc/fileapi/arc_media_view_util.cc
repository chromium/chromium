// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_media_view_util.h"

namespace arc {

namespace {

constexpr char kMediaViewVolumeIdPrefix[] = "media_view:";

}  // namespace

BASE_FEATURE(kMediaViewFeature,
             "ArcMediaView",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kMediaDocumentsProviderAuthority[] =
    "com.android.providers.media.documents";

const char kImagesRootId[] = "images_root";
const char kVideosRootId[] = "videos_root";
const char kAudioRootId[] = "audio_root";
const char kDocumentsRootId[] = "documents_root";

std::string GetMediaViewVolumeId(const std::string& root_id) {
  return std::string(kMediaViewVolumeIdPrefix) + root_id;
}

}  // namespace arc
