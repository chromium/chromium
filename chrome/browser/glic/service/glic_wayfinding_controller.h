// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_WAYFINDING_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_WAYFINDING_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"

class PrefService;

namespace glic {

// Tracks how a profile opens Glic, for the Ask Gemini Wayfinding cue, which
// teaches users who started from a secondary entry point (e.g. right-click)
// where the Ask Gemini button is. Only active when kGlicAskGeminiWayfinding
// is enabled.
class GlicWayfindingController {
 public:
  // `was_opened_before` is whether this profile had opened Glic before this
  // controller was created. Such profiles have no first invocation source.
  GlicWayfindingController(PrefService* pref_service, bool was_opened_before);
  GlicWayfindingController(const GlicWayfindingController&) = delete;
  GlicWayfindingController& operator=(const GlicWayfindingController&) = delete;
  ~GlicWayfindingController();

  // Called on every Glic open.
  void OnInvoke(mojom::InvocationSource source);

 private:
  raw_ptr<PrefService> pref_service_;
  const bool was_opened_before_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_WAYFINDING_CONTROLLER_H_
