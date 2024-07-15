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
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}
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
std::unique_ptr<KeyedService>
PrinterEventTrackerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  std::unique_ptr<PrinterEventTracker> tracker = std::make_unique<PrinterEventTracker>();
  tracker->set_logging(logging_enabled_);
  return tracker;
}

}  // namespace ash
