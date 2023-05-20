// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_about_handler.h"

#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/common/content_features.h"
#include "extensions/buildflags/buildflags.h"

bool HandleChromeAboutAndChromeSyncRewrite(
    GURL* url,
    content::BrowserContext* browser_context) {
  // Check that about: URLs are either
  // 1) fixed up to chrome: (by url_formatter::FixupURL applied to
  //    browser-initiated navigations)
  // or
  // 2) blocked (by content::RenderProcessHostImpl::FilterURL applied to
  //    renderer-initiated navigations)
  DCHECK(url->IsAboutBlank() || url->IsAboutSrcdoc() ||
         !url->SchemeIs(url::kAboutScheme));

  // Only handle chrome: URLs.
  if (!url->SchemeIs(content::kChromeUIScheme))
    return false;

  std::string host(url->host());
  if (host == chrome::kChromeUIAboutHost) {
    // Replace chrome://about with chrome://chrome-urls.
    host = chrome::kChromeUIChromeURLsHost;
  } else if (host == chrome::kChromeUISyncHost) {
    // Replace chrome://sync with chrome://sync-internals (for legacy reasons).
    host = chrome::kChromeUISyncInternalsHost;
  }

  if (host != url->host()) {
    GURL::Replacements replacements;
    replacements.SetHostStr(host);
    *url = url->ReplaceComponents(replacements);
  }

  // Having re-written the URL, make the chrome: handler process it.
  return false;
}

bool HandleNonNavigationAboutURL(const GURL& url) {
  if (!url.is_valid()) {
    return false;
  }
  const std::string spec(url.spec());

  if (base::EqualsCaseInsensitiveASCII(spec, chrome::kChromeUIRestartURL)) {
    // Call AttemptRestart after chrome::Navigate() completes to avoid access of
    // gtk objects after they are destroyed by BrowserWindowGtk::Close().
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&chrome::AttemptRestart));
    return true;
  }
  if (base::EqualsCaseInsensitiveASCII(spec, chrome::kChromeUIQuitURL)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&chrome::AttemptExit));
    return true;
  }

  return false;
}
