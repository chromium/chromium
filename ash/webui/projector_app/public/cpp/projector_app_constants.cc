// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"

namespace ash {

const char kChromeUIProjectorAppHost[] = "projector";
const char kChromeUIProjectorAnnotatorHost[] = "projector-annotator";

// content::WebUIDataSource::CreateAndAdd() requires trailing slash.
const char kChromeUIUntrustedProjectorUrl[] = "chrome-untrusted://projector/";

const char kChromeUIUntrustedAnnotatorUrl[] =
    "chrome-untrusted://projector-annotator/";

const char kChromeUITrustedProjectorSwaAppIdDeprecated[] =
    "nblbgfbmjfjaeonhjnbbkabkdploocij";

const base::FilePath::CharType kProjectorMetadataFileExtension[] =
    FILE_PATH_LITERAL(".projector");
const base::FilePath::CharType kProjectorV2MetadataFileExtension[] =
    FILE_PATH_LITERAL(".screencast");

const base::FilePath::CharType kProjectorMediaFileExtension[] =
    FILE_PATH_LITERAL(".webm");

const char kProjectorMediaMimeType[] = "video/webm";

const base::FilePath::CharType kScreencastDefaultThumbnailFileName[] =
    FILE_PATH_LITERAL("thumbnail.png");

}  // namespace ash
