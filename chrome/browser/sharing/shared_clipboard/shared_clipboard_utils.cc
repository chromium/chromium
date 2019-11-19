// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/shared_clipboard/feature_flags.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

bool ShouldOfferSharedClipboard(content::BrowserContext* browser_context,
                                const base::string16& text) {
  // Check Chrome enterprise policy for Shared Clipboard.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (profile &&
      !profile->GetPrefs()->GetBoolean(prefs::kSharedClipboardEnabled)) {
    return false;
  }

  SharingService* sharing_service =
      SharingServiceFactory::GetForBrowserContext(browser_context);
  return sharing_service && base::FeatureList::IsEnabled(kSharedClipboardUI) &&
         !text.empty();
}
