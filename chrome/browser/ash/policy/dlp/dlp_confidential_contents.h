// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DLP_CONFIDENTIAL_CONTENTS_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DLP_CONFIDENTIAL_CONTENTS_H_

#include <string>
#include <vector>

#include "ui/gfx/image/image_skia.h"

namespace content {
class WebContents;
}

namespace policy {

// Keeps track of title and corresponding icon of a WebContents object.
// Used to cache and later show information about observed confidential contents
// to the user.
struct DlpConfidentialContent {
  DlpConfidentialContent() = default;
  // Constructs DlpConfidentialContent from the title and icon obtained from
  // |web_contents|, which cannot be null.
  explicit DlpConfidentialContent(content::WebContents* web_contents);
  DlpConfidentialContent(const DlpConfidentialContent& other) = default;
  DlpConfidentialContent& operator=(const DlpConfidentialContent& other) =
      default;
  ~DlpConfidentialContent() = default;

  gfx::ImageSkia icon;
  std::u16string title;
};

using DlpConfidentialContents = std::vector<DlpConfidentialContent>;

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DLP_CONFIDENTIAL_CONTENTS_H_
