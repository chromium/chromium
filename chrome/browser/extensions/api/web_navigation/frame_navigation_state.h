// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_FRAME_NAVIGATION_STATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_FRAME_NAVIGATION_STATE_H_

#include "content/public/browser/document_user_data.h"
#include "url/gurl.h"

namespace content {
class RenderFrameHost;
}

namespace extensions {

// Used to track the loading state of RenderFrameHost instances in a given tab
// currently known to the webNavigation API. It is mainly used to track in which
// frames an error occurred so no further events for this frame are being sent.
// TODO(carlscab): DocumentState seems like a better name as this track per
// document state.
class FrameNavigationState
    : public content::DocumentUserData<FrameNavigationState> {
 public:
  FrameNavigationState(const FrameNavigationState&) = delete;
  FrameNavigationState& operator=(const FrameNavigationState&) = delete;

  ~FrameNavigationState() override;

  // True if in general webNavigation events may be sent for the given URL.
  static bool IsValidUrl(const GURL& url);

  // True if navigation events for the this frame can be sent.
  bool CanSendEvents() const;

  // Starts to track a document load to |url|.
  void StartTrackingDocumentLoad(const GURL& url,
                                 bool is_same_document,
                                 bool is_from_back_forward_cache,
                                 bool is_error_page);

  // Returns the URL corresponding to a tracked |frame_host|.
  // TODO(dcheng): Why is this needed? Can't this information be extracted from
  // RenderFrameHost?
  GURL GetUrl() const;

  // Marks this frame as in an error state, i.e. the onErrorOccurred event was
  // fired for it, and no further events should be sent for it.
  void SetErrorOccurredInFrame();

  // True if this frame is marked as being in an error state.
  bool GetErrorOccurredInFrame() const;

  // Marks this frame as having finished its last document load, i.e. the
  // onCompleted event was fired for this frame.
  void SetDocumentLoadCompleted();

  // True if this frame is currently not loading a document.
  bool GetDocumentLoadCompleted() const;

  // Marks this frame as having finished parsing.
  void SetParsingFinished();

  // True if |frame_host| has finished parsing.
  bool GetParsingFinished() const;

#ifdef UNIT_TEST
  static void set_allow_extension_scheme(bool allow_extension_scheme) {
    allow_extension_scheme_ = allow_extension_scheme;
  }
#endif

 private:
  friend class content::DocumentUserData<FrameNavigationState>;
  DOCUMENT_USER_DATA_KEY_DECL();

  explicit FrameNavigationState(content::RenderFrameHost*);

  bool error_occurred_ = false;  // True if an error has occurred in this frame.
  bool is_loading_ = false;      // True if there is a document load going on.
  bool is_parsing_ = false;      // True if the frame is still parsing.
  GURL url_;                     // URL of this frame.

  // If true, also allow events from chrome-extension:// URLs.
  static bool allow_extension_scheme_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_FRAME_NAVIGATION_STATE_H_
