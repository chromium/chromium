// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_INACTIVE_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_WIDGET_INACTIVE_VIEW_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_tracker.h"

namespace content {
class WebContents;
}

namespace gfx {
class SlideAnimation;
}

namespace views {
class ImageView;
}  // namespace views

// Manages a view that displays a blurred and dynamically resized screenshot.
// This class encapsulates the creation of the view hierarchy and the logic for
// updating the blurred image when the view's bounds change.
class InactiveViewController : public views::ViewObserver,
                               public gfx::AnimationDelegate {
 public:
  InactiveViewController();
  ~InactiveViewController() override;

  InactiveViewController(const InactiveViewController&) = delete;
  InactiveViewController& operator=(const InactiveViewController&) = delete;

  // Creates and returns the view managed by this controller. The controller
  // retains a raw_ptr to the ImageView within the hierarchy to update it.
  std::unique_ptr<views::View> CreateView();

  void CaptureScreenshot(content::WebContents* glic_webui_contents);

  // Sets the screenshot to be displayed. This will trigger the initial blur and
  // display of the image.
  void OnScreenshotCaptured(gfx::Image screenshot);

  base::WeakPtr<InactiveViewController> GetWeakPtr();

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;
  void OnViewThemeChanged(views::View* observed_view) override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

 private:
  // Updates the displayed image by resizing and re-blurring the screenshot.
  void UpdateImageView();

  // Updates the scrim color based on the current theme.
  void UpdateScrimColor();

  // Updates the scrim opacity based on the animation value.
  void UpdateScrimOpacity(double animation_value);

  // Checks if the aspect ratio difference requires blurring the image.
  void CheckForImageDistortion();

  base::ScopedObservation<views::View, views::ViewObserver>
      image_view_observation_{this};
  raw_ptr<views::ImageView> image_view_ = nullptr;
  views::ViewTracker scrim_view_tracker_;
  gfx::ImageSkia screenshot_;
  bool is_image_distorted_ = false;

  std::unique_ptr<gfx::SlideAnimation> animation_;
  views::ViewTracker card_view_tracker_;

  base::WeakPtrFactory<InactiveViewController> weak_ptr_factory_{this};

  // Creates and returns the card view.
  std::unique_ptr<views::View> CreateCardView();
};

#endif  // CHROME_BROWSER_GLIC_WIDGET_INACTIVE_VIEW_CONTROLLER_H_
