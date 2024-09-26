// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/presentation/chrome_local_presentation_manager_factory.h"

#include "base/lazy_instance.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace media_router {

namespace {

base::LazyInstance<ChromeLocalPresentationManagerFactory>::DestructorAtExit
    service_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
ChromeLocalPresentationManagerFactory*
ChromeLocalPresentationManagerFactory::GetInstance() {
  return &service_factory.Get();
}

ChromeLocalPresentationManagerFactory::ChromeLocalPresentationManagerFactory() =
    default;
ChromeLocalPresentationManagerFactory::
    ~ChromeLocalPresentationManagerFactory() = default;

content::BrowserContext*
ChromeLocalPresentationManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace media_router
