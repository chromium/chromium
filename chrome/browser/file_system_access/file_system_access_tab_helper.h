// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_TAB_HELPER_H_
#define CHROME_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"

// This class keeps track of tabs as they're being created, navigated and
// destroyed, informing the permission context when an origin is navigated away
// from. This is then used by the permission context to revoke permissions when
// no top level tabs remain for an origin.
class FileSystemAccessTabHelper : public content::WebContentsObserver {
 public:
  explicit FileSystemAccessTabHelper(content::WebContents* web_contents);
  FileSystemAccessTabHelper(const FileSystemAccessTabHelper&) = delete;
  FileSystemAccessTabHelper& operator=(const FileSystemAccessTabHelper&) =
      delete;
  ~FileSystemAccessTabHelper() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;
};

#endif  // CHROME_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_TAB_HELPER_H_
