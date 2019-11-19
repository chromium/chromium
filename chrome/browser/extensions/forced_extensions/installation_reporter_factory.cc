// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/installation_reporter_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/extensions/forced_extensions/installation_reporter.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace extensions {

// static
InstallationReporter* InstallationReporterFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<InstallationReporter*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
InstallationReporterFactory* InstallationReporterFactory::GetInstance() {
  static base::NoDestructor<InstallationReporterFactory> instance;
  return instance.get();
}

InstallationReporterFactory::InstallationReporterFactory()
    : BrowserContextKeyedServiceFactory(
          "InstallationReporter",
          BrowserContextDependencyManager::GetInstance()) {}

InstallationReporterFactory::~InstallationReporterFactory() = default;

KeyedService* InstallationReporterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new InstallationReporter(context);
}

}  // namespace extensions
