// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LENS_LENS_OVERLAY_LENS_OVERLAY_URL_BUILDER_H_
#define CHROME_BROWSER_LENS_LENS_OVERLAY_LENS_OVERLAY_URL_BUILDER_H_

#include <string>

#include "url/gurl.h"

namespace lens {
GURL AppendCommonSearchParametersToURL(const GURL& url_to_modify);
GURL BuildSearchURL(const std::string& text_query);
}  // namespace lens

#endif  // CHROME_BROWSER_LENS_LENS_OVERLAY_LENS_OVERLAY_URL_BUILDER_H_
