// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager_lacros.h"

namespace policy {

namespace {
static DlpContentManagerLacros* g_dlp_content_manager = nullptr;
}  // namespace

// static
DlpContentManagerLacros* DlpContentManagerLacros::Get() {
  if (!g_dlp_content_manager) {
    g_dlp_content_manager = new DlpContentManagerLacros();
  }
  return g_dlp_content_manager;
}

void DlpContentManagerLacros::OnConfidentialityChanged(
    content::WebContents* web_contents,
    const DlpContentRestrictionSet& restriction_set) {
  // TODO(crbug.com/1254331): Implement for LaCros.
}

void DlpContentManagerLacros::OnWebContentsDestroyed(
    content::WebContents* web_contents) {
  // TODO(crbug.com/1254331): Implement for LaCros.
}

DlpContentRestrictionSet DlpContentManagerLacros::GetRestrictionSetForURL(
    const GURL& url) const {
  DlpContentRestrictionSet set;
  // TODO(crbug.com/1254331): Implement for LaCros.
  return set;
}

void DlpContentManagerLacros::OnVisibilityChanged(
    content::WebContents* web_contents) {
  // TODO(crbug.com/1254331): Implement for LaCros.
}

}  // namespace policy
