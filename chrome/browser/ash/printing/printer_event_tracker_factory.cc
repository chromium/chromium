// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/printer_event_tracker_factory.h"

#include "base/lazy_instance.h"
#include "chrome/browser/ash/printing/printer_event_tracker.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace ash {
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
    : ProfileKeyedServiceFactory(
          "PrinterEventTracker",
          ProfileSelections::BuildForRegularAndIncognito()) {}
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

}  // namespace ash
