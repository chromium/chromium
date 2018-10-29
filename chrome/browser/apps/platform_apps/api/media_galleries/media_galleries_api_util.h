// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_API_MEDIA_GALLERIES_MEDIA_GALLERIES_API_UTIL_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_API_MEDIA_GALLERIES_MEDIA_GALLERIES_API_UTIL_H_

#include <memory>

#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace chrome_apps {
namespace api {

// Converts a mojo media metadata struct into a dictionary. Internally uses
// extension's auto generated serializer.
std::unique_ptr<base::DictionaryValue> SerializeMediaMetadata(
    chrome::mojom::MediaMetadataPtr metadata);

}  // namespace api
}  // namespace chrome_apps

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_API_MEDIA_GALLERIES_MEDIA_GALLERIES_API_UTIL_H_
