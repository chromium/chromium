// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/unhandled_tap_web_contents_observer.h"

namespace contextual_search {

UnhandledTapWebContentsObserver::UnhandledTapWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

UnhandledTapWebContentsObserver::~UnhandledTapWebContentsObserver() {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(UnhandledTapWebContentsObserver)

}  // namespace contextual_search
