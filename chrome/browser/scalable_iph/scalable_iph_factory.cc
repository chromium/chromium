// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/scalable_iph/scalable_iph_factory.h"

#include "base/logging.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace {

constexpr char kScalableIphServiceName[] = "ScalableIphKeyedService";

ScalableIphFactory* g_scalable_iph_factory = nullptr;

}  // namespace

ScalableIphFactory::ScalableIphFactory()
    : BrowserContextKeyedServiceFactory(
          kScalableIphServiceName,
          BrowserContextDependencyManager::GetInstance()) {
  CHECK(!g_scalable_iph_factory);
  g_scalable_iph_factory = this;
}

ScalableIphFactory::~ScalableIphFactory() {
  CHECK(g_scalable_iph_factory);
  g_scalable_iph_factory = nullptr;
}

ScalableIphFactory* ScalableIphFactory::GetInstance() {
  CHECK(g_scalable_iph_factory)
      << "ScalableIphFactory instance must be instantiated by "
         "ScalableIphFactoryImpl::BuildInstance()";
  return g_scalable_iph_factory;
}

scalable_iph::ScalableIph* ScalableIphFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<scalable_iph::ScalableIph*>(
      // Service must be created via `InitializeServiceForProfile`.
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 /*create=*/false));
}

void ScalableIphFactory::InitializeServiceForProfile(Profile* profile) {
  // TODO(b/286604737): Disables ScalableIph services if multi-user sign-in is
  // used.

  // Create a `ScalableIph` service to start a timer for time tick event. Ignore
  // a return value. It can be nullptr if the browser context (i.e. profile) is
  // not eligible for `ScalableIph`.
  GetServiceForBrowserContext(profile, /*create=*/true);
}
