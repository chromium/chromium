// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_BLURRED_SCREENSHOT_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_WIDGET_BLURRED_SCREENSHOT_VIEW_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace views {
class ImageView;
}  // namespace views

// Manages a view that displays a blurred and dynamically resized screenshot.
// This class encapsulates the creation of the view hierarchy and the logic for
// updating the blurred image when the view's bounds change.
class BlurredScreenshotViewController : public views::ViewObserver {
 public:
  BlurredScreenshotViewController();
  ~BlurredScreenshotViewController() override;

  BlurredScreenshotViewController(const BlurredScreenshotViewController&) =
      delete;
  BlurredScreenshotViewController& operator=(
      const BlurredScreenshotViewController&) = delete;

  // Creates and returns the view managed by this controller. The controller
  // retains a raw_ptr to the ImageView within the hierarchy to update it.
  std::unique_ptr<views::View> CreateView();

  // Sets the screenshot to be displayed. This will trigger the initial blur and
  // display of the image.
  void OnScreenshotCaptured(gfx::Image screenshot);

  base::WeakPtr<BlurredScreenshotViewController> GetWeakPtr();

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

 private:
  // Updates the displayed image by resizing and re-blurring the screenshot.
  void UpdateImageView();

  base::ScopedObservation<views::View, views::ViewObserver>
      image_view_observation_{this};
  raw_ptr<views::ImageView> image_view_ = nullptr;
  gfx::ImageSkia screenshot_;

  base::WeakPtrFactory<BlurredScreenshotViewController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_GLIC_WIDGET_BLURRED_SCREENSHOT_VIEW_CONTROLLER_H_
