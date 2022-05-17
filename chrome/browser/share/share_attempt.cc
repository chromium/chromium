// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/share_attempt.h"

#include "content/public/browser/web_contents.h"

namespace share {

ShareAttempt::ShareAttempt(content::WebContents* contents)
    : ShareAttempt(contents, contents->GetTitle(), contents->GetVisibleURL()) {}
ShareAttempt::ShareAttempt(content::WebContents* contents,
                           std::u16string title,
                           GURL url)
    : web_contents(contents->GetWeakPtr()), title(title), url(url) {}

ShareAttempt::~ShareAttempt() = default;

ShareAttempt::ShareAttempt(const ShareAttempt&) = default;

}  // namespace share
