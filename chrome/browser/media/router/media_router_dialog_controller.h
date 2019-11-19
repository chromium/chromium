// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/media/router/presentation/presentation_service_delegate_impl.h"
#include "chrome/common/media_router/mojom/media_router.mojom.h"
#include "content/public/browser/presentation_request.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"

namespace content {
class WebContents;
}  // namespace content

namespace media_router {

// An abstract base class for Media Router dialog controllers. Tied to a
// WebContents known as the |initiator|, and is lazily created when a Media
// Router dialog needs to be shown. The MediaRouterDialogController allows
// showing and closing a Media Router dialog modal to the initiator WebContents.
// This class is not thread safe and must be called on the UI thread.
class MediaRouterDialogController {
 public:
  virtual ~MediaRouterDialogController();

  // Gets a reference to the MediaRouterDialogController associated with
  // |web_contents|, creating one if it does not exist. The returned pointer is
  // guaranteed to be non-null. This method has platform-specific
  // implementations in directories such as chrome/browser/ui/views/ and
  // chrome/browser/media/android/.
  static MediaRouterDialogController* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  // Shows the media router dialog modal to |initiator_|, with additional
  // context for a PresentationRequest coming from the page given by the input
  // parameters.
  // Returns true if the dialog is created as a result of this call.
  // If the dialog already exists, or dialog cannot be created, then false is
  // returned, and |error_cb| will be invoked.
  bool ShowMediaRouterDialogForPresentation(
      std::unique_ptr<StartPresentationContext> context);

  // Shows the media router dialog modal to |initiator_|.
  // Creates the dialog if it did not exist prior to this call, returns true.
  // If the dialog already exists, brings it to the front, returns false.
  virtual bool ShowMediaRouterDialog();

  // Hides the media router dialog.
  // It is a no-op to call this function if there is currently no dialog.
  void HideMediaRouterDialog();

  // Indicates if the media router dialog already exists.
  virtual bool IsShowingMediaRouterDialog() const = 0;

 protected:
  // Use MediaRouterDialogController::GetOrCreateForWebContents() to create an
  // instance.
  explicit MediaRouterDialogController(content::WebContents* initiator);

  // Creates a media router dialog if necessary, then activates the WebContents
  // that initiated the dialog, e.g. focuses the tab.
  void FocusOnMediaRouterDialog(bool dialog_needs_creation);

  // Returns the WebContents that initiated showing the dialog.
  content::WebContents* initiator() const { return initiator_; }

  // Resets the state of the controller. Must be called from the overrides.
  virtual void Reset();
  // Creates a new media router dialog modal to |initiator_|.
  virtual void CreateMediaRouterDialog() = 0;
  // Closes the media router dialog if it exists.
  virtual void CloseMediaRouterDialog() = 0;

  // Data for dialogs created at the request of the Presentation API.
  // Created from arguments passed in via ShowMediaRouterDialogForPresentation.
  std::unique_ptr<StartPresentationContext> start_presentation_context_;

 private:
  class InitiatorWebContentsObserver;

  // An observer for the |initiator_| that closes the dialog when |initiator_|
  // is destroyed or navigated.
  std::unique_ptr<InitiatorWebContentsObserver> initiator_observer_;
  content::WebContents* const initiator_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterDialogController);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_H_
