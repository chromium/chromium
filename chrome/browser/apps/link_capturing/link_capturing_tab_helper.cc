// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/link_capturing_tab_helper.h"

#include <utility>

#include "content/public/browser/web_contents_user_data.h"

namespace apps {

LinkCapturingTabHelper::~LinkCapturingTabHelper() {}

LinkCapturingTabHelper::LinkCapturingTabHelper(content::WebContents* contents,
                                               webapps::AppId source_app_id)
    : content::WebContentsUserData<LinkCapturingTabHelper>(*contents),
      source_app_id_(std::move(source_app_id)) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LinkCapturingTabHelper);

}  // namespace apps
