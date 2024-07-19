// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LENS_REGION_SEARCH_LENS_REGION_SEARCH_CONTROLLER_H_
#define CHROME_BROWSER_LENS_REGION_SEARCH_LENS_REGION_SEARCH_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/image_editor/screenshot_flow.h"
#include "components/lens/lens_metrics.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/image/image.h"
#include "ui/views/widget/widget.h"

namespace content {
class WebContents;
enum class Visibility;
}  // namespace content

namespace views {
class Widget;
}  // namespace views

namespace lens {

class LensRegionSearchController : public content::WebContentsObserver {
 public:
  LensRegionSearchController();
  ~LensRegionSearchController() override;

  // Creates and runs the drag and capture flow. When run, the user enters into
  // a screenshot capture mode with the ability to draw a rectagular region
  // around the web contents. When finished with selection, the region is
  // converted into a PNG and sent to Lens. If `use_fullscreen_capture` is set
  // to true, the whole screen will automatically be captured.
  void Start(content::WebContents* web_contents,
             bool use_fullscreen_capture,
             bool force_open_in_new_tab,
             bool is_google_default_search_provider,
             lens::AmbientSearchEntryPoint entry_point);

  // Closes the UI overlay and user education bubble if currently being shown.
  // The closed reason for this method is defaulted to the close button being
  // clicked.
  void Close();
  void Escape();

  // Closes the UI overlay and user education bubble if shown with the specified
  // closed reason.
  void CloseWithReason(views::Widget::ClosedReason reason);

  // Calculates the percentage that the image area takes up in the screen area.
  // This value is calculated as double and then floored to the nearest integer.
  static int CalculateViewportProportionFromAreas(int screen_height,
                                                  int screen_width,
                                                  int image_width,
                                                  int image_height);
  // Returns an enum representing the aspect ratio of the image as defined in
  // lens_metrics.h.
  static lens::LensRegionSearchAspectRatio GetAspectRatioFromSize(
      int image_height,
      int image_width);

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // The function handling the metrics recording and resizing that happens when
  // the capture has been completed.
  void OnCaptureCompleted(const image_editor::ScreenshotCaptureResult& result);

  // Returns whether the overlay and instruction bubble are both visible. If
  // either of the UI elements is not visible, returns false.
  bool IsOverlayUIVisibleForTesting();
  void SetEntryPointForTesting(lens::AmbientSearchEntryPoint entry_point);

  // Sets the web contents for unit tests that do not launch the region search
  // UI.
  void SetWebContentsForTesting(content::WebContents* web_contents);

 private:
  void RecordCaptureResult(lens::LensRegionSearchCaptureResult result);

  void RecordRegionSizeRelatedMetrics(gfx::Rect screen_bounds,
                                      gfx::Size region_size);

  // Variable for tracking the default search provider as to launch the image
  // results in correct search engine. This value is set every time the capture
  // mode is started to have an accurate value for the completed capture.
  bool is_google_default_search_provider_ = false;

  // Variable for tracking whether the region search request originated from the
  // companion.
  lens::AmbientSearchEntryPoint entry_point_;

  // Variable for tracking whether or not to force the region search to open
  // results in a new tab, instead of the side panel.
  bool force_open_in_new_tab_ = false;

  bool in_capture_mode_ = false;

  std::unique_ptr<image_editor::ScreenshotFlow> screenshot_flow_;

  raw_ptr<views::Widget> bubble_widget_ = nullptr;

  base::WeakPtr<LensRegionSearchController> weak_this_;

  base::WeakPtrFactory<LensRegionSearchController> weak_factory_{this};
};

// Class to associate region search controller data with Profile across
// navigation. Used to support region search via keyboard shortcut.
class LensRegionSearchControllerData : public base::SupportsUserData::Data {
 public:
  LensRegionSearchControllerData();
  ~LensRegionSearchControllerData() override;
  LensRegionSearchControllerData(const LensRegionSearchControllerData&) =
      delete;
  LensRegionSearchControllerData& operator=(
      const LensRegionSearchControllerData&) = delete;

  static constexpr char kDataKey[] = "lens_region_search_controller_data";
  std::unique_ptr<LensRegionSearchController> lens_region_search_controller;
};

// Class to associate region search captured data with Profile across
// navigation. Used to support region search on a static WebUI page.
class RegionSearchCapturedData : public base::SupportsUserData::Data {
 public:
  RegionSearchCapturedData();
  ~RegionSearchCapturedData() override;
  RegionSearchCapturedData(const RegionSearchCapturedData&) = delete;
  RegionSearchCapturedData& operator=(const RegionSearchCapturedData&) = delete;

  static constexpr char kDataKey[] = "region_search_data";
  gfx::Image image;
};
}  // namespace lens
#endif  // CHROME_BROWSER_LENS_REGION_SEARCH_LENS_REGION_SEARCH_CONTROLLER_H_
