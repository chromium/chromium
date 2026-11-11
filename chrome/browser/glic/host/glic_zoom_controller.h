// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_ZOOM_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_ZOOM_CONTROLLER_H_

#include <array>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/host/glic_webui.mojom-forward.h"

class PrefService;

namespace content {
class WebContents;
}

namespace pwc {
class PrivilegedWebContents;
}

namespace glic {

// Manages zoom level scaling, persistence, and isolation for the Glic guest
// PrivilegedWebContents.
//
// Discrete zoom steps range between 100% (1.0) and 200% (2.0), matching the
// historical webview zoom factor table. Zoom is applied via
// PrivilegedWebContents::SetOverrideZoomLevel() (exposed through
// WebContentsDelegate::GetOverrideZoomLevel()) so initial navigations carry the
// saved zoom immediately without leaking zoom modifications to
// gemini.google.com in standard browser tabs.
class GlicZoomController {
 public:
  // LINT.IfChange(GlicZoomFactors)
  static constexpr std::array<double, 6> kZoomFactors = {1.0, 1.1,  1.25,
                                                         1.5, 1.75, 2.0};
  // LINT.ThenChange(//chrome/browser/glic/host/guest_util.cc:GlicZoomFactors)

  static constexpr double kZoomDeltaThreshold = 0.01;

  GlicZoomController(pwc::PrivilegedWebContents* guest_contents,
                     PrefService* pref_service,
                     base::RepeatingClosure on_zoom_change_callback = {});
  ~GlicZoomController();
  GlicZoomController(const GlicZoomController&) = delete;
  GlicZoomController& operator=(const GlicZoomController&) = delete;

  // Executes a zoom action (kZoomIn, kZoomOut, kReset) from the given source.
  void Zoom(mojom::ZoomAction zoom_action, ZoomSource source);

  // Returns the current zoom factor (e.g. 1.0, 1.1, 1.25).
  double GetCurrentZoomFactor() const;

  // Applies the current zoom factor to the guest WebContents as a zoom level
  // override.
  void ApplyZoom();

 private:
  base::WeakPtr<content::WebContents> guest_contents_;
  raw_ptr<PrefService> pref_service_;
  base::RepeatingClosure on_zoom_change_callback_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_ZOOM_CONTROLLER_H_
