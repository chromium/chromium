// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_tab_observer.h"

#include "privacy_sandbox_tab_observer.h"

namespace privacy_sandbox {

PrivacySandboxTabObserver::PrivacySandboxTabObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

PrivacySandboxTabObserver::~PrivacySandboxTabObserver() = default;

void PrivacySandboxTabObserver::PrimaryPageChanged(content::Page& page) {
  // TODO(crbug.com/308320418): Surface the `always on` survey.
}

}  // namespace privacy_sandbox
