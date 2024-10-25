// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_client.h"

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

ContextualCueingClient::ContextualCueingClient(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ContextualCueingClient>(*web_contents) {}

ContextualCueingClient::~ContextualCueingClient() = default;

// content::WebContentsObserver
void ContextualCueingClient::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextualCueingClient);
