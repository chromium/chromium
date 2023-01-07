// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_OS_FEEDBACK_OS_FEEDBACK_SCREENSHOT_MANAGER_H_
#define CHROME_BROWSER_ASH_OS_FEEDBACK_OS_FEEDBACK_SCREENSHOT_MANAGER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"

namespace ash {

// This is a singleton class used to manage screenshot taking and cleanup.
//
// The reasons it is needed are as the followings:
// 1) We wanted to take a screenshot first before launching the feedback SWA.
// 2) Currently, SWA factory does not accept a custom object reference specific
// to a SWA.
//
// If we start a thread to take screenshot while the SWA is being launched, we
// may run into a race condition such that the screenshot may include the UI of
// the feedback SWA itself which is not desirable.
//
// With this class, we can intercept the launching process of the feedback SWA,
// hide it, take a screenshot, and when done, show the SWA. The SWA can
// retrieve the screenshot any time and clean it up upon exits.
class OsFeedbackScreenshotManager {
 public:
  OsFeedbackScreenshotManager(const OsFeedbackScreenshotManager&) = delete;
  OsFeedbackScreenshotManager& operator=(const OsFeedbackScreenshotManager&) =
      delete;

  using ScreenshotCallback = base::OnceCallback<void(bool)>;

  // Get the instance of this class. If not exists, will create one first.
  static OsFeedbackScreenshotManager* GetInstance();
  // Return null if not exists. Otherwise, return the instance.
  static OsFeedbackScreenshotManager* GetIfExists();
  // Take a screenshot of the primary display if any and persist the data in a
  // field.
  // Returns true in callback if screenshot is taken.
  // Returns false in callback if screenshot is not taken or failed.
  void TakeScreenshot(ScreenshotCallback callback);
  // Return the screenshot png data or nullptr if screenshot can't be taken.
  scoped_refptr<base::RefCountedMemory> GetScreenshotData();
  // Remove the screenshot data. It is expected to be called when the feedback
  // SWA exits.
  void DeleteScreenshotData();

  void SetPngDataForTesting(scoped_refptr<base::RefCountedMemory> png_data) {
    screenshot_png_data_ = std::move(png_data);
  }

 private:
  OsFeedbackScreenshotManager();
  ~OsFeedbackScreenshotManager();
  friend struct base::DefaultSingletonTraits<OsFeedbackScreenshotManager>;

  void OnAllScreenshotsTaken(
      ScreenshotCallback callback,
      std::vector<scoped_refptr<base::RefCountedMemory>> all_data);

  scoped_refptr<base::RefCountedMemory> screenshot_png_data_;
  base::WeakPtrFactory<OsFeedbackScreenshotManager> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_OS_FEEDBACK_OS_FEEDBACK_SCREENSHOT_MANAGER_H_
