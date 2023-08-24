// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal_util.h"
#include "url/gurl.h"

namespace safe_browsing {

std::string SanitizeURLWithoutFilename(std::string url) {
  return GURL(url).GetWithoutFilename().spec();
}

}  // namespace safe_browsing
