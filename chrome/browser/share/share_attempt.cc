// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/share_attempt.h"

#include "chrome/browser/favicon/favicon_utils.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/image_model.h"

namespace share {

ShareAttempt::ShareAttempt(content::WebContents* contents)
    : ShareAttempt(contents,
                   contents->GetTitle(),
                   contents->GetVisibleURL(),
                   ui::ImageModel::FromImage(favicon::GetDefaultFavicon())) {}
ShareAttempt::ShareAttempt(content::WebContents* contents,
                           std::u16string title,
                           GURL url,
                           ui::ImageModel preview_image)
    : web_contents(contents ? contents->GetWeakPtr() : nullptr),
      title(title),
      url(url),
      preview_image(preview_image) {}

ShareAttempt::~ShareAttempt() = default;

ShareAttempt::ShareAttempt(const ShareAttempt&) = default;

}  // namespace share
