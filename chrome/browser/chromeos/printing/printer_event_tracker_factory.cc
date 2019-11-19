// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printer_event_tracker_factory.h"

#include "base/lazy_instance.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/printing/printer_event_tracker.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {
namespace {
base::LazyInstance<PrinterEventTrackerFactory>::DestructorAtExit
    g_printer_tracker = LAZY_INSTANCE_INITIALIZER;
}  // namespace

// static
PrinterEventTrackerFactory* PrinterEventTrackerFactory::GetInstance() {
  return g_printer_tracker.Pointer();
}

// static
PrinterEventTracker* PrinterEventTrackerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<PrinterEventTracker*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

PrinterEventTrackerFactory::PrinterEventTrackerFactory()
    : BrowserContextKeyedServiceFactory(
          "PrinterEventTracker",
          BrowserContextDependencyManager::GetInstance()) {}
PrinterEventTrackerFactory::~PrinterEventTrackerFactory() = default;

void PrinterEventTrackerFactory::SetLogging(bool enabled) {
  logging_enabled_ = enabled;
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (Profile* profile : profiles) {
    auto* tracker = static_cast<PrinterEventTracker*>(
        GetServiceForBrowserContext(profile, false));
    if (tracker) {
      tracker->set_logging(logging_enabled_);
    }
  }
}

// BrowserContextKeyedServiceFactory:
KeyedService* PrinterEventTrackerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  PrinterEventTracker* tracker = new PrinterEventTracker();
  tracker->set_logging(logging_enabled_);
  return tracker;
}

content::BrowserContext* PrinterEventTrackerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace chromeos
