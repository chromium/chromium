// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/incognito_helpers.h"

#include "chrome/browser/profiles/profile.h"

content::BrowserContext* GetBrowserContextRedirectedInIncognito(
    content::BrowserContext* context) {
  return Profile::FromBrowserContext(context)->GetOriginalProfile();
}

const content::BrowserContext* GetBrowserContextRedirectedInIncognito(
    const content::BrowserContext* context) {
  const Profile* profile = Profile::FromBrowserContext(
      const_cast<content::BrowserContext*>(context));
  return profile->GetOriginalProfile();
}

content::BrowserContext* GetBrowserContextOwnInstanceInIncognito(
    content::BrowserContext* context) {
  return context;
}
