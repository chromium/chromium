// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_about_handler.h"

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/common/content_features.h"
#include "extensions/buildflags/buildflags.h"

bool FixupBrowserAboutURL(GURL* url,
                          content::BrowserContext* browser_context) {
  // Ensure that any cleanup done by FixupURL happens before the rewriting
  // phase that determines the virtual URL, by including it in an initial
  // URLHandler.  This prevents minor changes from producing a virtual URL,
  // which could lead to a URL spoof.
  *url = url_formatter::FixupURL(url->possibly_invalid_spec(), std::string());
  return true;
}

bool WillHandleBrowserAboutURL(GURL* url,
                               content::BrowserContext* browser_context) {
  // TODO(msw): Eliminate "about:*" constants and literals from code and tests,
  //            then hopefully we can remove this forced fixup.
  FixupBrowserAboutURL(url, browser_context);

  // Check that about: URLs are fixed up to chrome: by url_formatter::FixupURL.
  DCHECK(url->IsAboutBlank() || url->IsAboutSrcdoc() ||
         !url->SchemeIs(url::kAboutScheme));

  // Only handle chrome://foo/, url_formatter::FixupURL translates about:foo.
  if (!url->SchemeIs(content::kChromeUIScheme))
    return false;

  std::string host(url->host());
  std::string path;

  // Handle chrome://settings.
  if (host == chrome::kChromeUISettingsHost)
    return true;  // Prevent further rewriting - this is a valid URL.

  // Do not handle chrome://help.
  if (host == chrome::kChromeUIHelpHost)
    return false;  // Handled in the HandleWebUI handler.

  // Replace about with chrome-urls.
  if (host == chrome::kChromeUIAboutHost)
    host = chrome::kChromeUIChromeURLsHost;

  if (host == chrome::kChromeUISyncHost) {
    // Replace sync with sync-internals (for legacy reasons).
    host = chrome::kChromeUISyncInternalsHost;
// Redirect chrome://extensions and chrome://settings/extensions all to
// chrome://extensions and forward path.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  } else if (host == chrome::kChromeUIExtensionsHost ||
             (host == chrome::kChromeUISettingsHost &&
              url->path() ==
                  std::string("/") + chrome::kDeprecatedExtensionsSubPage)) {
    host = chrome::kChromeUIExtensionsHost;
    path = url->path();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  } else if (host == chrome::kChromeUIHistoryHost) {
    // Redirect chrome://history.
    path = url->path();
  }

  GURL::Replacements replacements;
  replacements.SetHostStr(host);
  if (!path.empty())
    replacements.SetPathStr(path);
  *url = url->ReplaceComponents(replacements);

  // Having re-written the URL, make the chrome: handler process it.
  return false;
}

bool HandleNonNavigationAboutURL(const GURL& url) {
  const std::string spec(url.spec());

  if (base::LowerCaseEqualsASCII(spec, chrome::kChromeUIRestartURL)) {
    // Call AttemptRestart after chrome::Navigate() completes to avoid access of
    // gtk objects after they are destroyed by BrowserWindowGtk::Close().
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&chrome::AttemptRestart));
    return true;
  }
  if (base::LowerCaseEqualsASCII(spec, chrome::kChromeUIQuitURL)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&chrome::AttemptExit));
    return true;
  }

  return false;
}
