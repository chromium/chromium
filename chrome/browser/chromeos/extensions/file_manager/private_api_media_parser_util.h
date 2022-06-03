// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MEDIA_PARSER_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MEDIA_PARSER_UTIL_H_

#include <memory>

#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace extensions {

namespace api {

namespace file_manager_private {

// Converts a mojo::MediaMetadata to a MediaMetadata value.
std::unique_ptr<base::DictionaryValue> MojoMediaMetadataToValue(
    chrome::mojom::MediaMetadataPtr metadata);

}  // namespace file_manager_private

}  // namespace api

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MEDIA_PARSER_UTIL_H_
