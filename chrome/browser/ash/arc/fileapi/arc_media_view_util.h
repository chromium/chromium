// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities for ARC Media View.

#ifndef CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_MEDIA_VIEW_UTIL_H_
#define CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_MEDIA_VIEW_UTIL_H_

#include <string>

namespace arc {

// Authority of MediaDocumentsProvider in Android.
extern const char kMediaDocumentsProviderAuthority[];

// Root IDs of file system roots in MediaDocumentsProvider.
extern const char kImagesRootId[];
extern const char kVideosRootId[];
extern const char kAudioRootId[];
extern const char kDocumentsRootId[];

// Returns an ID of a Media View volume.
std::string GetMediaViewVolumeId(const std::string& root_id);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_MEDIA_VIEW_UTIL_H_
