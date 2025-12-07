// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_TAB_OBSERVER_HELPER_BASE_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_TAB_OBSERVER_HELPER_BASE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"

namespace content {
class WebContents;
}  // namespace content

// Base class for platform-specific helpers that observe tab activation changes
// for Auto Picture-in-Picture.
class AutoPictureInPictureTabObserverHelperBase {
 public:
  using ActivatedChangedCallback =
      base::RepeatingCallback<void(bool is_tab_activated)>;

  // Factory method to create the platform-specific helper.
  static std::unique_ptr<AutoPictureInPictureTabObserverHelperBase> Create(
      content::WebContents* web_contents,
      ActivatedChangedCallback callback);

  AutoPictureInPictureTabObserverHelperBase(content::WebContents* web_contents,
                                            ActivatedChangedCallback callback);

  virtual ~AutoPictureInPictureTabObserverHelperBase();

  // Starts observing the tab model associated with the WebContents provided
  // during construction.
  virtual void StartObserving() = 0;

  // Stops observing.
  virtual void StopObserving() = 0;

  // Returns the WebContents of the currently active tab in the observed model.
  virtual content::WebContents* GetActiveWebContents() const = 0;

 protected:
  void RunCallback(bool is_tab_activated);

  content::WebContents* GetObservedWebContents() const { return web_contents_; }

 private:
  // The WebContents instance that this helper is observing for activation
  // changes (i.e., becoming the active tab or ceasing to be the active tab).
  raw_ptr<content::WebContents> web_contents_;
  ActivatedChangedCallback callback_;
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_TAB_OBSERVER_HELPER_BASE_H_
