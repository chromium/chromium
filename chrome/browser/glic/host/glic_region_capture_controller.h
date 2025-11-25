// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_REGION_CAPTURE_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_REGION_CAPTURE_CONTROLLER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class WebContents;
}  // namespace content

namespace lens {
class LensRegionSearchController;
}

namespace glic {

class GlicRegionCaptureController {
 public:
  GlicRegionCaptureController();
  ~GlicRegionCaptureController();

  void CaptureRegion(
      content::WebContents* web_contents,
      mojo::PendingRemote<mojom::CaptureRegionObserver> observer);
  void CancelCaptureRegion();
  bool IsCaptureRegionInProgressForTesting() const;

  void SetOnCaptureRegionForTesting(base::RepeatingClosure cb) {
    on_capture_region_for_testing_ = std::move(cb);
  }

  // These methods are public for testing purposes.
  void OnRegionSelected(const gfx::Rect& rect);
  void OnRegionSelectionFlowClosed();

 private:
  void ResetMembers();
  void OnCaptureRegionObserverDisconnected();

  base::RepeatingClosure on_capture_region_for_testing_ = base::DoNothing();

  raw_ptr<content::WebContents> web_contents_ = nullptr;

  std::unique_ptr<lens::LensRegionSearchController>
      lens_region_search_controller_;
  mojo::Remote<mojom::CaptureRegionObserver> capture_region_observer_;

  base::WeakPtrFactory<GlicRegionCaptureController> weak_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_REGION_CAPTURE_CONTROLLER_H_
