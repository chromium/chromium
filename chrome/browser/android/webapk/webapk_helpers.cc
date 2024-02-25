// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_helpers.h"

#include "components/crx_file/id_util.h"
#include "components/webapps/common/web_app_id.h"
#include "crypto/sha2.h"
#include "url/gurl.h"

namespace webapk {

webapps::AppId GenerateAppIdFromManifestId(
    const webapps::ManifestId& manifest_id) {
  // The app ID is hashed twice: here and in GenerateId.
  // The double-hashing is for historical reasons and it needs to stay
  // this way for backwards compatibility.
  //
  // This must stay the same as the dPWA implementation in
  // chrome/browser/web_applications/web_app_helpers.cc's
  // GenerateAppIdFromManifestId().
  return crx_file::id_util::GenerateId(
      crypto::SHA256HashString(manifest_id.spec()));
}

}  // namespace webapk
