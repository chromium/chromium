// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_FRAME_NAVIGATION_STATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_FRAME_NAVIGATION_STATE_H_

#include <map>
#include <set>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "url/gurl.h"

namespace content {
class RenderFrameHost;
}

namespace extensions {

// Tracks the loading state of all frame hosts in a given tab currently known
// to the webNavigation API. It is mainly used to track in which frames an error
// occurred so no further events for this frame are being sent.
class FrameNavigationState {
 public:
  typedef std::set<content::RenderFrameHost*>::const_iterator const_iterator;

  FrameNavigationState();
  ~FrameNavigationState();

  // True if in general webNavigation events may be sent for the given URL.
  static bool IsValidUrl(const GURL& url);

  // Use these to iterate over all frame hosts known by this object.
  const_iterator begin() const { return frame_hosts_.begin(); }
  const_iterator end() const { return frame_hosts_.end(); }

  // True if navigation events for the given frame can be sent.
  bool CanSendEvents(content::RenderFrameHost* frame_host) const;

  // Starts to track a document load in |frame_host| to |url|.
  void StartTrackingDocumentLoad(content::RenderFrameHost* frame_host,
                                 const GURL& url,
                                 bool is_same_document,
                                 bool is_error_page);

  // Adds the |frame_host| to the set of RenderFrameHosts associated with the
  // WebContents this object is tracking. This method and FrameHostDeleted
  // are used to track the set of current RenderFrameHosts, which is used to
  // implement the GetFrame and GetAllFrames extension APIs.
  void FrameHostCreated(content::RenderFrameHost* frame_host);

  // Removes the |frame_host| from the set of RenderFrameHosts associated with
  // the WebContents this object is tracking.
  void FrameHostDeleted(content::RenderFrameHost* frame_host);

  // Returns true if |frame_host| is a known frame host.
  bool IsValidFrame(content::RenderFrameHost* frame_host) const;

  // Returns the URL corresponding to a tracked |frame_host|.
  // TODO(dcheng): Why is this needed? Can't this information be extracted from
  // RenderFrameHost?
  GURL GetUrl(content::RenderFrameHost* frame_host) const;

  // Marks |frame_host| as in an error state, i.e. the onErrorOccurred event was
  // fired for it, and no further events should be sent for it.
  void SetErrorOccurredInFrame(content::RenderFrameHost* frame_host);

  // True if |frame_host| is marked as being in an error state.
  bool GetErrorOccurredInFrame(content::RenderFrameHost* frame_host) const;

  // Marks |frame_host| as having finished its last document load, i.e. the
  // onCompleted event was fired for this frame.
  void SetDocumentLoadCompleted(content::RenderFrameHost* frame_host);

  // True if |frame_host| is currently not loading a document.
  bool GetDocumentLoadCompleted(content::RenderFrameHost* frame_host) const;

  // Marks |frame_host| as having finished parsing.
  void SetParsingFinished(content::RenderFrameHost* frame_host);

  // True if |frame_host| has finished parsing.
  bool GetParsingFinished(content::RenderFrameHost* frame_host) const;

#ifdef UNIT_TEST
  static void set_allow_extension_scheme(bool allow_extension_scheme) {
    allow_extension_scheme_ = allow_extension_scheme;
  }
#endif

 private:
  struct FrameState {
    FrameState();

    bool error_occurred;  // True if an error has occurred in this frame.
    bool is_loading;        // True if there is a document load going on.
    bool is_parsing;  // True if the frame is still parsing.
    GURL url;  // URL of this frame.
  };
  typedef std::map<content::RenderFrameHost*, FrameState> FrameHostToStateMap;

  // Tracks the state of known frame hosts.
  FrameHostToStateMap frame_host_state_map_;

  // Set of all known frame hosts.
  std::set<content::RenderFrameHost*> frame_hosts_;

  // If true, also allow events from chrome-extension:// URLs.
  static bool allow_extension_scheme_;

  DISALLOW_COPY_AND_ASSIGN(FrameNavigationState);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_FRAME_NAVIGATION_STATE_H_
