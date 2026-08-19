// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/cross_tab_copy_paste_tracker_factory.h"

#include "components/autofill/core/browser/metrics/cross_tab_copy_paste_tracker.h"

namespace autofill {

// static
CrossTabCopyPasteTracker* CrossTabCopyPasteTrackerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<CrossTabCopyPasteTracker*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
CrossTabCopyPasteTrackerFactory*
CrossTabCopyPasteTrackerFactory::GetInstance() {
  static base::NoDestructor<CrossTabCopyPasteTrackerFactory> instance;
  return instance.get();
}

CrossTabCopyPasteTrackerFactory::CrossTabCopyPasteTrackerFactory()
    : ProfileKeyedServiceFactory(
          "CrossTabCopyPasteTracker",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .Build()) {}

CrossTabCopyPasteTrackerFactory::~CrossTabCopyPasteTrackerFactory() = default;

std::unique_ptr<KeyedService>
CrossTabCopyPasteTrackerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<CrossTabCopyPasteTracker>();
}

}  // namespace autofill
