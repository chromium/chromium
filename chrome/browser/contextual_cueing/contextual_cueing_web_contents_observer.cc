// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_web_contents_observer.h"

#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "content/public/browser/web_contents.h"

namespace contextual_cueing {

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextualCueingWebContentsObserver);

ContextualCueingWebContentsObserver::ContextualCueingWebContentsObserver(
    content::WebContents* web_contents,
    ContextualCueingService* service)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ContextualCueingWebContentsObserver>(
          *web_contents),
      service_(service) {}

ContextualCueingWebContentsObserver::~ContextualCueingWebContentsObserver() =
    default;

void ContextualCueingWebContentsObserver::PrimaryPageChanged(
    content::Page& page) {
  service_->ReportPageLoad();
}

}  // namespace contextual_cueing
