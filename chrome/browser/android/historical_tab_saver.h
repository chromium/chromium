// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_HISTORICAL_TAB_SAVER_H_
#define CHROME_BROWSER_ANDROID_HISTORICAL_TAB_SAVER_H_

#include <memory>

class TabAndroid;
namespace content {
class WebContents;
}  // namespace content

namespace historical_tab_saver {

// A wrapper to manage a web contents of a possibly frozen tab.
class ScopedWebContents {
 public:
  // Returns a wrapped WebContents for `tab`. If the tab has a live web contents
  // it will be used. If one does not exist (e.g. the tab is frozen) then a
  // temporary one will be created from the frozen tab's WebContentsState. If
  // the WebContents was created from a frozen tab it will be destroyed with the
  // returned object.
  static std::unique_ptr<ScopedWebContents> CreateForTab(TabAndroid* tab);

  ~ScopedWebContents();

  ScopedWebContents(const ScopedWebContents&) = delete;
  ScopedWebContents& operator=(const ScopedWebContents&) = delete;

  content::WebContents* web_contents() const { return web_contents_; }

 private:
  ScopedWebContents(content::WebContents* web_contents, bool was_frozen);

  content::WebContents* web_contents_;
  bool was_frozen_;
};

}  // namespace historical_tab_saver

#endif  // CHROME_BROWSER_ANDROID_HISTORICAL_TAB_SAVER_H_
