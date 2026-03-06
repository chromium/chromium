// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/finds/core/finds_tab_helper.h"

#include "chrome/browser/finds/core/finds_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"

namespace finds {

FindsTabHelper::FindsTabHelper(content::WebContents* web_contents,
                               FindsService* finds_service)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<FindsTabHelper>(*web_contents) {
  CHECK(finds_service);
  finds_service_ = finds_service;
}

FindsTabHelper::~FindsTabHelper() = default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(FindsTabHelper);

}  // namespace finds
