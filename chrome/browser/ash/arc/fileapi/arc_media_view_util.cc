// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_media_view_util.h"

namespace arc {

namespace {

constexpr char kMediaViewVolumeIdPrefix[] = "media_view:";

}  // namespace

const base::Feature kMediaViewFeature{"ArcMediaView",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

const char kMediaDocumentsProviderAuthority[] =
    "com.android.providers.media.documents";

const char kImagesRootDocumentId[] = "images_root";
const char kVideosRootDocumentId[] = "videos_root";
const char kAudioRootDocumentId[] = "audio_root";

std::string GetMediaViewVolumeId(const std::string& root_document_id) {
  return std::string(kMediaViewVolumeIdPrefix) + root_document_id;
}

}  // namespace arc
