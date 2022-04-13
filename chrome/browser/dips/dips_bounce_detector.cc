// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_bounce_detector.h"

using content::NavigationHandle;

DIPSBounceDetector::DIPSBounceDetector(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<DIPSBounceDetector>(*web_contents) {}

void DIPSBounceDetector::DidRedirectNavigation(NavigationHandle* handle) {
  // TODO: detect if bounce is stateful and if so fire UKM metric
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DIPSBounceDetector);
