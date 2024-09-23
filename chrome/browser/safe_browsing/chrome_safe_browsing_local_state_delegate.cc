// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_safe_browsing_local_state_delegate.h"

#include "chrome/browser/browser_process.h"
ChromeSafeBrowsingLocalStateDelegate::ChromeSafeBrowsingLocalStateDelegate(
    content::WebUI* web_ui) {}
PrefService* ChromeSafeBrowsingLocalStateDelegate::GetLocalState() {
  return g_browser_process->local_state();
}
