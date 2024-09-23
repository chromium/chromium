// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_ANDROID_DATA_PROVIDER_H_
#define CHROME_BROWSER_ANDROID_TAB_ANDROID_DATA_PROVIDER_H_

#include "chrome/browser/tab/web_contents_state.h"
#include "components/sessions/core/session_id.h"

// Pure virtual interface for the backing tab object from Java and associated
// JNI bridge class.
class TabAndroidDataProvider {
 public:
  // Return specific id information regarding this tab.
  virtual SessionID GetWindowId() const = 0;

  // Returns the tab id generated/tracked by logic in Java.
  virtual int GetAndroidId() const = 0;

  // May return nullptr, which caller should handle accordingly.
  virtual std::unique_ptr<WebContentsStateByteBuffer>
  GetWebContentsByteBuffer() = 0;

 protected:
  virtual ~TabAndroidDataProvider() = default;
};

#endif  // CHROME_BROWSER_ANDROID_TAB_ANDROID_DATA_PROVIDER_H_
