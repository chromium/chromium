// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"

#include "content/public/browser/web_contents.h"

ContextualSearchWebContentsHelper::ContextualSearchWebContentsHelper(
    content::WebContents* web_contents)
    : content::WebContentsUserData<ContextualSearchWebContentsHelper>(
          *web_contents) {}

ContextualSearchWebContentsHelper::~ContextualSearchWebContentsHelper() =
    default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextualSearchWebContentsHelper);
