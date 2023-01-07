// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_AVATAR_MENU_OBSERVER_H_
#define CHROME_BROWSER_PROFILES_AVATAR_MENU_OBSERVER_H_

class AvatarMenu;

// Delegate interface for objects that want to be notified when the
// AvatarMenu changes.
class AvatarMenuObserver {
 public:
  virtual ~AvatarMenuObserver() {}

  virtual void OnAvatarMenuChanged(AvatarMenu* avatar_menu) = 0;
};

#endif  // CHROME_BROWSER_PROFILES_AVATAR_MENU_OBSERVER_H_
