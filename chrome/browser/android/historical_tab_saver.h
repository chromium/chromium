// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_HISTORICAL_TAB_SAVER_H_
#define CHROME_BROWSER_ANDROID_HISTORICAL_TAB_SAVER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/pickle.h"

class TabAndroid;
namespace content {
class WebContents;
}  // namespace content

namespace historical_tab_saver {

// A struct to store the WebContentsState passed down from the JNI to be
// potentially used in restoring a frozen tab, as a byte buffer.
struct WebContentsStateByteBuffer {
  WebContentsStateByteBuffer();
  ~WebContentsStateByteBuffer();
  /**
   * @param data WebContentsState data stored as a byte buffer.
   * @param size Byte buffer size.
   * @param saved_state_version Saved state version of the WebContentsState.
   */
  WebContentsStateByteBuffer(void* data, int size, int saved_state_version);

  base::Pickle byte_buffer;
  int state_version;
};

// A wrapper to manage a web contents of a possibly frozen tab.
class ScopedWebContents {
 public:
  // Returns a wrapped WebContents for `tab`. If the tab has a live web contents
  // it will be used. If one does not exist (e.g. the tab is frozen) then a
  // temporary one will be created from the frozen tab's WebContentsState. If
  // the WebContents was created from a frozen tab it will be destroyed with the
  // returned object.
  static std::unique_ptr<ScopedWebContents> CreateForTab(
      TabAndroid* tab,
      const WebContentsStateByteBuffer* webContentsStateByteBuffer);

  ~ScopedWebContents();

  ScopedWebContents(const ScopedWebContents&) = delete;
  ScopedWebContents& operator=(const ScopedWebContents&) = delete;

  explicit ScopedWebContents(content::WebContents* unowned_web_contents);
  explicit ScopedWebContents(
      std::unique_ptr<content::WebContents> owned_web_contents);

  content::WebContents* web_contents() const;

 private:
  raw_ptr<content::WebContents> unowned_web_contents_;
  std::unique_ptr<content::WebContents> owned_web_contents_;
};

}  // namespace historical_tab_saver

#endif  // CHROME_BROWSER_ANDROID_HISTORICAL_TAB_SAVER_H_
