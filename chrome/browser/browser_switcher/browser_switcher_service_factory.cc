// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_service_factory.h"

#include "build/build_config.h"
#include "chrome/browser/browser_switcher/browser_switcher_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

#if defined(OS_WIN)
#include "chrome/browser/browser_switcher/browser_switcher_service_win.h"
#endif

namespace browser_switcher {

namespace {

#if defined(OS_WIN)
using BrowserSwitcherServiceImpl = BrowserSwitcherServiceWin;
#else
using BrowserSwitcherServiceImpl = BrowserSwitcherService;
#endif

}  // namespace

// static
BrowserSwitcherServiceFactory* BrowserSwitcherServiceFactory::GetInstance() {
  return base::Singleton<BrowserSwitcherServiceFactory>::get();
}

// static
BrowserSwitcherService* BrowserSwitcherServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<BrowserSwitcherService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

BrowserSwitcherServiceFactory::BrowserSwitcherServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "BrowserSwitcherServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {}

BrowserSwitcherServiceFactory::~BrowserSwitcherServiceFactory() {}

KeyedService* BrowserSwitcherServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* instance =
      new BrowserSwitcherServiceImpl(Profile::FromBrowserContext(context));
  instance->Init();
  return instance;
}

content::BrowserContext* BrowserSwitcherServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Use the original profile's BrowserSwitcherService, even in Incognito mode.
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace browser_switcher
