// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_io_data.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/url_constants.h"
#include "components/dom_distiller/core/url_constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "net/net_buildflags.h"
#include "third_party/blink/public/common/features.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/net/nss_service_chromeos_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using content::BrowserThread;

void ProfileIOData::InitializeOnUIThread(Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // This triggers ResourceContext creation and initializes the per-profile NSS
  // database.
  //
  // TODO(https://crbug.com/1018972): Remove this line once NSS initialization
  // no longer depends on the ResourceContext.
  NssServiceChromeOSFactory::GetForContext(profile);
#else  // !BUILDFLAG(IS_CHROMEOS_ASH)
  // Make sure the ResourceContext is initialized.  It's unclear if this is
  // still needed.
  content::BrowserContext::EnsureResourceContextInitialized(profile);
#endif
}

ProfileIOData::ProfileIOData()
    : resource_context_(std::make_unique<content::ResourceContext>()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ProfileIOData::~ProfileIOData() {
  if (BrowserThread::IsThreadInitialized(BrowserThread::IO)) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    content::GetIOThreadTaskRunner({})->DeleteSoon(
        FROM_HERE, std::move(resource_context_));
  }
}

// static
bool ProfileIOData::IsHandledProtocol(const std::string& scheme) {
  DCHECK_EQ(scheme, base::ToLowerASCII(scheme));
  static const char* const kProtocolList[] = {
    url::kHttpScheme,
    url::kHttpsScheme,
#if BUILDFLAG(ENABLE_WEBSOCKETS)
    url::kWsScheme,
    url::kWssScheme,
#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)
    url::kFileScheme,
    content::kChromeDevToolsScheme,
    dom_distiller::kDomDistillerScheme,
#if BUILDFLAG(ENABLE_EXTENSIONS)
    extensions::kExtensionScheme,
#endif
    content::kChromeUIScheme,
    content::kChromeUIUntrustedScheme,
    url::kDataScheme,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    content::kExternalFileScheme,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if defined(OS_ANDROID)
    url::kContentScheme,
#endif  // defined(OS_ANDROID)
    url::kAboutScheme,
    url::kBlobScheme,
    url::kFileSystemScheme,
    chrome::kChromeSearchScheme,
  };
  for (const char* supported_protocol : kProtocolList) {
    if (scheme == supported_protocol)
      return true;
  }
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  if (scheme == url::kFtpScheme &&
      base::FeatureList::IsEnabled(blink::features::kFtpProtocol)) {
    return true;
  }
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)
  return false;
}

// static
bool ProfileIOData::IsHandledURL(const GURL& url) {
  if (!url.is_valid()) {
    // We handle error cases.
    return true;
  }

  return IsHandledProtocol(url.scheme());
}

content::ResourceContext* ProfileIOData::GetResourceContext() const {
  return resource_context_.get();
}
