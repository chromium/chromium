// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_MANAGER_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_MANAGER_H_

#include <functional>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "build/build_config.h"
#include "third_party/blink/public/mojom/picture_in_picture_window_options/picture_in_picture_window_options.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_border.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/types/pass_key.h"
#include "chrome/browser/picture_in_picture/auto_pip_setting_overlay_view.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace content {
enum class PictureInPictureResult;
class PictureInPictureWindowController;
class WebContents;
}  // namespace content

namespace display {
class Display;
}  // namespace display

#if !BUILDFLAG(IS_ANDROID)
class PictureInPictureOcclusionTracker;
class ScopedDisallowPictureInPicture;

namespace views {
class View;
}  // namespace views
#endif

struct NavigateParams;

// PictureInPictureWindowManager is a singleton that handles the lifetime of the
// current Picture-in-Picture window and its PictureInPictureWindowController.
// The class also guarantees that only one window will be present per Chrome
// instances regardless of the number of windows, tabs, profiles, etc.
class PictureInPictureWindowManager {
 public:
  // Observer for PictureInPictureWindowManager events.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnEnterPictureInPicture() {}
  };

  // Returns the singleton instance.
  static PictureInPictureWindowManager* GetInstance();

  PictureInPictureWindowManager(const PictureInPictureWindowManager&) = delete;
  PictureInPictureWindowManager& operator=(
      const PictureInPictureWindowManager&) = delete;

  // Shows a PIP window using the window controller for a video element.
  //
  // This mode is triggered through WebContentsDelegate::EnterPictureInPicture,
  // and the default implementation of that fails with a kNotSupported
  // result. For compatibility, this method must also return a
  // content::PictureInPictureResult even though it doesn't fail.
  content::PictureInPictureResult EnterVideoPictureInPicture(
      content::WebContents*);

#if !BUILDFLAG(IS_ANDROID)
  // Shows a PIP window using the window controller for document picture in
  // picture.
  //
  // Document picture-in-picture mode is triggered from the Renderer via
  // WindowOpenDisposition::NEW_PICTURE_IN_PICTURE, and the browser
  // (i.e. Chrome's BrowserNavigator) then calls this method to create the
  // window. There's no corresponding path through the WebContentsDelegate, so
  // it doesn't have a failure state.
  void EnterDocumentPictureInPicture(content::WebContents* parent_web_contents,
                                     content::WebContents* child_web_contents);
#endif  // !BUILDFLAG(IS_ANDROID)

  // Shows a PIP window with an explicitly provided window controller. This is
  // used by ChromeOS ARC windows which do not have a WebContents as the source.
  void EnterPictureInPictureWithController(
      content::PictureInPictureWindowController* pip_window_controller);

  // Expected behavior of the window UI-initiated close.
  enum class UiBehavior {
    // Close the window, but don't try to pause the video.  This is also the
    // behavior of `ExitPictureInPicture()`.
    kCloseWindowOnly,

    // Close the window, and also pause the video.
    kCloseWindowAndPauseVideo,

    // Act like the back-to-tab button: focus the opener window, and don't pause
    // the video.
    kCloseWindowAndFocusOpener,
  };

  // The user has requested to close the pip window.  This is similar to
  // `ExitPictureInPicture()`, except that it's strictly user-initiated via the
  // window UI.
  bool ExitPictureInPictureViaWindowUi(UiBehavior behavior);

  // Closes any existing picture-in-picture windows (video or document pip).
  // Returns true if a picture-in-picture window was closed, and false if there
  // were no picture-in-picture windows to close.
  bool ExitPictureInPicture();

  // Called to notify that the initiator web contents should be focused.
  void FocusInitiator();

  // Gets the web contents in the opener browser window.
  content::WebContents* GetWebContents() const;

  // Gets the web contents in the PiP window. This only applies to document PiP
  // and will be null for video PiP.
  content::WebContents* GetChildWebContents() const;

  // Helper method that will check if the given WebContents is hosted in a
  // document PiP window.
  static bool IsChildWebContents(content::WebContents*);

  // Returns the window bounds of the video picture-in-picture or the document
  // picture-in-picture if either of them is present.
  std::optional<gfx::Rect> GetPictureInPictureWindowBounds() const;

  // Used for Document picture-in-picture windows only. The returned dimensions
  // represent the outer window bounds.
  // This method is called from the |BrowserNavigator|. Note that the window
  // bounds may be later re-adjusted by the |PictureInPictureBrowserFrameView|
  // to accommodate non-client view elements, while respecting the minimum inner
  // window size.
  gfx::Rect CalculateInitialPictureInPictureWindowBounds(
      const blink::mojom::PictureInPictureWindowOptions& pip_options,
      const display::Display& display);

  // Used for Document picture-in-picture windows only. The returned dimensions
  // represent the outer window bounds.
  // This method is called from |PictureInPictureBrowserFrameView|. Picture in
  // picture window bounds are only adjusted when, the requested window size
  // would cause the minimum inner window size to be smaller than the allowed
  // minimum (|GetMinimumInnerWindowSize|).
  gfx::Rect CalculateOuterWindowBounds(
      const blink::mojom::PictureInPictureWindowOptions& pip_options,
      const display::Display& display,
      const gfx::Size& minimum_window_size,
      const gfx::Size& excluded_margin);

  // Update the most recent window bounds for the pip window in the cache.  Call
  // this when the pip window moves or resizes, though it's okay if not every
  // update makes it here.
  void UpdateCachedBounds(const gfx::Rect& most_recent_bounds);

  // Used for Document picture-in-picture windows only.
  // Note that this is meant to represent the inner window bounds. When the pip
  // window is drawn, outer bounds may be greater than kMinWindowSize to
  // accommodate window decorations and ensure the inner bound minimum size
  // respects kMinWindowSize.
  static gfx::Size GetMinimumInnerWindowSize();

  // Used for Document picture-in-picture windows only.
  static gfx::Size GetMaximumWindowSize(const display::Display& display);

  // Properly sets the `window_action` on `params`.
  static void SetWindowParams(NavigateParams& params);

  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }
  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  // Notify observers that picture-in-picture window is created.
  void NotifyObserversOnEnterPictureInPicture();

#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<AutoPipSettingOverlayView> GetOverlayView(
      views::View* anchor_view,
      views::BubbleBorder::Arrow arrow);

  // Returns the PictureInPictureOcclusionTracker, which can inform observers
  // when a widget has been occluded by a video or document picture-in-picture
  // window.
  PictureInPictureOcclusionTracker* GetOcclusionTracker();

  // Returns true if a file dialog opened by `owner_web_contents` should create
  // a `ScopedDisallowPictureInPicture` to block picture-in-picture.
  bool ShouldFileDialogBlockPictureInPicture(
      content::WebContents* owner_web_contents);

  // Called by `ScopedDisallowPictureInPicture` to block picture-in-picture
  // windows from opening, as well as close any existing picture-in-picture
  // windows.
  void OnScopedDisallowPictureInPictureCreated(
      base::PassKey<ScopedDisallowPictureInPicture>);
  void OnScopedDisallowPictureInPictureDestroyed(
      base::PassKey<ScopedDisallowPictureInPicture>);

  // Returns true if picture-in-picture is currently disabled (e.g. due to a
  // ScopedDisallowPictureInPicture object existing).
  bool IsPictureInPictureDisabled() const;
#endif

  void set_window_controller_for_testing(
      content::PictureInPictureWindowController* controller) {
    pip_window_controller_ = controller;
  }

  // Return true if and only if the URL is can be used as an opener.  This check
  // allows us to explicitly opt-in opener URL types, to ensure that the pip
  // window's title bar is formatted properly.  Allowing any secure context, for
  // example, might result in a misleading window title.
  static bool IsSupportedForDocumentPictureInPicture(const GURL& url);

 private:
  friend struct base::DefaultSingletonTraits<PictureInPictureWindowManager>;
  class VideoWebContentsObserver;
#if !BUILDFLAG(IS_ANDROID)
  class DocumentWebContentsObserver;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(PictureInPictureDisallowedType)
  enum class PictureInPictureDisallowedType {
    // An existing picture-in-picture window was closed because we started
    // disallowing picture-in-picture windows.
    kExistingWindowClosed = 0,

    // A new picture-in-picture window was closed because it was created while
    // we were already disallowing picture-in-picture windows.
    kNewWindowClosed = 1,

    kMaxValue = kNewWindowClosed,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/media/enums.xml:PictureInPictureDisallowedTypeEnum)
#endif  // !BUILDFLAG(IS_ANDROID)

  // Create a Picture-in-Picture window and register it in order to be closed
  // when needed.
  // This is suffixed with "Internal" because `CreateWindow` is part of the
  // Windows API.
  void CreateWindowInternal(content::WebContents*);

  // Closes the active Picture-in-Picture window.
  // There MUST be a window open.
  // This is suffixed with "Internal" to keep consistency with the method above.
  void CloseWindowInternal();

#if !BUILDFLAG(IS_ANDROID)
  // Called when the document PiP parent web contents is being destroyed.
  void DocumentWebContentsDestroyed();
#endif  // !BUILDFLAG(IS_ANDROID)

  // Exits picture in picture soon, but not before this call returns.  If
  // picture in picture closes between now and then, that's okay.  Intended as a
  // helper class for callbacks, to avoid re-entrant calls during pip set-up.
  static void ExitPictureInPictureSoon();

#if !BUILDFLAG(IS_ANDROID)
  // Creates the `occlusion_tracker_` if it does not already exist and should
  // exist.
  void CreateOcclusionTrackerIfNecessary();

  // Records metrics about the requested size of a document picture-in-picture
  // window.
  void RecordDocumentPictureInPictureRequestedSizeMetrics(
      const blink::mojom::PictureInPictureWindowOptions& pip_options,
      const display::Display& display);

  // Records whether a new or existing picture-in-picture window was closed due
  // to an existing ScopedDisallowPictureInPicture.
  void RecordPictureInPictureDisallowed(PictureInPictureDisallowedType type);
#endif  // !BUILDFLAG(IS_ANDROID)

  PictureInPictureWindowManager();
  ~PictureInPictureWindowManager();

  // Observers that listen to updates of this instance.
  base::ObserverList<Observer> observers_;

  std::unique_ptr<VideoWebContentsObserver> video_web_contents_observer_;
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<DocumentWebContentsObserver> document_web_contents_observer_;

  std::unique_ptr<PictureInPictureOcclusionTracker> occlusion_tracker_;

  // The number of `ScopedDisallowPictureInPicture` objects currently in
  // existence. If at least one exists, then picture-in-picture windows will be
  // blocked.
  uint32_t number_of_existing_scoped_disallow_picture_in_pictures_ = 0;
#endif  //! BUILDFLAG(IS_ANDROID)

  raw_ptr<content::PictureInPictureWindowController, DanglingUntriaged>
      pip_window_controller_ = nullptr;
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_MANAGER_H_
