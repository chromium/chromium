// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

bool ShouldOfferSharedClipboard(content::BrowserContext* browser_context,
                                const std::u16string& text) {
  // TODO(https://crbug.com/1311675): Remove this function and all its call
  // sites.
  return false;
}
