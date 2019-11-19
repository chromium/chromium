// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_LIST_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_LIST_OBSERVER_H_

class DesktopMediaList;

// Interface implemented by the desktop media picker dialog to receive
// notifications about changes in DesktopMediaList.
class DesktopMediaListObserver {
 public:
  // TODO(jrw): None of the |list| parameters below seem to be used.  Consider
  // removing them.
  virtual void OnSourceAdded(DesktopMediaList* list, int index) = 0;
  virtual void OnSourceRemoved(DesktopMediaList* list, int index) = 0;
  virtual void OnSourceMoved(DesktopMediaList* list,
                             int old_index,
                             int new_index) = 0;
  virtual void OnSourceNameChanged(DesktopMediaList* list, int index) = 0;
  virtual void OnSourceThumbnailChanged(DesktopMediaList* list, int index) = 0;

 protected:
  virtual ~DesktopMediaListObserver() {}
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_LIST_OBSERVER_H_
