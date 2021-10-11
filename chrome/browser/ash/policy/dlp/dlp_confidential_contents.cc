// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_confidential_contents.h"

#include "chrome/browser/favicon/favicon_utils.h"
#include "content/public/browser/web_contents.h"

namespace policy {

DlpConfidentialContent::DlpConfidentialContent(
    content::WebContents* web_contents)
    : icon(favicon::TabFaviconFromWebContents(web_contents).AsImageSkia()),
      title(web_contents->GetTitle()) {}

}  // namespace policy
