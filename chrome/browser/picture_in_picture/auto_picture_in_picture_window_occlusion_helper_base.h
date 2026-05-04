// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_WINDOW_OCCLUSION_HELPER_BASE_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_WINDOW_OCCLUSION_HELPER_BASE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"

namespace content {
class WebContents;
}  // namespace content

// Base class for platform-specific helpers that observe window occlusion
// changes for auto picture-in-picture.
class AutoPictureInPictureWindowOcclusionHelperBase {
 public:
  enum class OcclusionState {
    // The observed WebContents's BrowserWindow is currently visible to the
    // user.
    kVisible,

    // The observed WebContents's BrowserWindow is currently blocked by another
    // window.
    kOccluded,

    // The observed WebContent's BrowserWindow is hidden from the user (e.g. due
    // to being minimized).
    kHidden,
  };

  using OcclusionStateChangedCallback =
      base::RepeatingCallback<void(OcclusionState occlusion_state)>;

  // Factory method to create the platform-specific helper.
  static std::unique_ptr<AutoPictureInPictureWindowOcclusionHelperBase> Create(
      content::WebContents* web_contents,
      OcclusionStateChangedCallback callback);

  using FactoryCallback = base::RepeatingCallback<
      std::unique_ptr<AutoPictureInPictureWindowOcclusionHelperBase>(
          content::WebContents* web_contents,
          OcclusionStateChangedCallback callback)>;

  // Sets the factory function to be used in place of the platform-specific
  // `Create()` for testing.
  static void SetFactoryForTesting(FactoryCallback factory);

  AutoPictureInPictureWindowOcclusionHelperBase(
      content::WebContents* web_contents,
      OcclusionStateChangedCallback callback);

  virtual ~AutoPictureInPictureWindowOcclusionHelperBase();

  // Starts observing the window for occlusion state changes.
  virtual void StartObserving() = 0;

  // Stops observing.
  virtual void StopObserving() = 0;

  // Returns the current occlusion state.
  virtual OcclusionState GetOcclusionState() const = 0;

 protected:
  void RunCallback(OcclusionState occlusion_state);

  content::WebContents* GetObservedWebContents() const { return web_contents_; }

  // Factory method to create the platform-specific helper.
  static std::unique_ptr<AutoPictureInPictureWindowOcclusionHelperBase>
  CreatePlatformHelper(content::WebContents* web_contents,
                       OcclusionStateChangedCallback callback);

 private:
  const raw_ptr<content::WebContents> web_contents_;
  OcclusionStateChangedCallback callback_;
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_WINDOW_OCCLUSION_HELPER_BASE_H_
