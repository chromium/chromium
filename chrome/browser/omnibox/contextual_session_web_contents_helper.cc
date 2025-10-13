// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/omnibox/contextual_session_web_contents_helper.h"

#include "content/public/browser/web_contents.h"

ContextualSessionWebContentsHelper::ContextualSessionWebContentsHelper(
    content::WebContents* web_contents)
    : content::WebContentsUserData<ContextualSessionWebContentsHelper>(
          *web_contents) {}

ContextualSessionWebContentsHelper::~ContextualSessionWebContentsHelper() =
    default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextualSessionWebContentsHelper);
