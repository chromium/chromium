// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PROJECTOR_APP_PUBLIC_CPP_PROJECTOR_APP_CONSTANTS_H_
#define ASH_WEBUI_PROJECTOR_APP_PUBLIC_CPP_PROJECTOR_APP_CONSTANTS_H_

#include "base/files/file_path.h"

namespace ash {

extern const char kChromeUIProjectorAppHost[];
extern const char kChromeUIProjectorAnnotatorHost[];

extern const char kChromeUIUntrustedProjectorUrl[];
extern const char kChromeUIUntrustedProjectorPwaUrl[];

// The ID of the Projector SWA.
extern const char kChromeUIUntrustedProjectorSwaAppId[];

// The deprecated app-id of the Projector SWA.
extern const char kChromeUITrustedProjectorSwaAppIdDeprecated[];

extern const char kChromeUIUntrustedAnnotatorUrl[];

// File extension of Projector metadata file. It is used to identify Projector
// screencasts at processing pending screencasts and fetching screencast list.
extern const base::FilePath::CharType kProjectorMetadataFileExtension[];

// File extension of Projector V2 metadata file.
extern const base::FilePath::CharType kProjectorV2MetadataFileExtension[];

// File extension of Projector media file.
extern const base::FilePath::CharType kProjectorMediaFileExtension[];

extern const char kProjectorMediaMimeType[];

// Default name of screencast thumbnail file.
extern const base::FilePath::CharType kScreencastDefaultThumbnailFileName[];

}  // namespace ash

#endif  // ASH_WEBUI_PROJECTOR_APP_PUBLIC_CPP_PROJECTOR_APP_CONSTANTS_H_
