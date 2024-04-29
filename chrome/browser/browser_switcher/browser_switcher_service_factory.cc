// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_service_factory.h"

#include "build/build_config.h"
#include "chrome/browser/browser_switcher/browser_switcher_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/browser_switcher/browser_switcher_service_win.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#error BrowserSwitcher is not supported on ChromeOS. Neither Ash nor LaCrOS.
#endif

namespace browser_switcher {

namespace {

#if BUILDFLAG(IS_WIN)
using BrowserSwitcherServiceImpl = BrowserSwitcherServiceWin;
#else
using BrowserSwitcherServiceImpl = BrowserSwitcherService;
#endif

}  // namespace

// static
BrowserSwitcherServiceFactory* BrowserSwitcherServiceFactory::GetInstance() {
  static base::NoDestructor<BrowserSwitcherServiceFactory> instance;
  return instance.get();
}

// static
BrowserSwitcherService* BrowserSwitcherServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<BrowserSwitcherService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

BrowserSwitcherServiceFactory::BrowserSwitcherServiceFactory()
    : ProfileKeyedServiceFactory("BrowserSwitcherServiceFactory",
                                 // Only create BrowserSwitcherService for
                                 // regular, non-Incognito profiles.
                                 ProfileSelections::BuildForRegularProfile()) {}

BrowserSwitcherServiceFactory::~BrowserSwitcherServiceFactory() = default;

std::unique_ptr<KeyedService>
BrowserSwitcherServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  std::unique_ptr<BrowserSwitcherServiceImpl> instance =
      std::make_unique<BrowserSwitcherServiceImpl>(Profile::FromBrowserContext(context));
  instance->Init();
  return instance;
}

}  // namespace browser_switcher
