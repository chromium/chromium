// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MEDIA_PARSER_UTIL_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MEDIA_PARSER_UTIL_H_

#include "base/values.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom-forward.h"

namespace extensions {

namespace api {

namespace file_manager_private {

// Converts a mojo::MediaMetadata to a MediaMetadata value.
base::Value::Dict MojoMediaMetadataToValue(
    chrome::mojom::MediaMetadataPtr metadata);

}  // namespace file_manager_private

}  // namespace api

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MEDIA_PARSER_UTIL_H_
