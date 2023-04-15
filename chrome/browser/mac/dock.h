// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_DOCK_H_
#define CHROME_BROWSER_MAC_DOCK_H_

#if defined(__OBJC__)
@class NSString;
#endif

namespace dock {

enum ChromeInDockStatus {
  ChromeInDockFailure,
  ChromeInDockFalse,
  ChromeInDockTrue
};

enum AddIconStatus {
  IconAddFailure,
  IconAddSuccess,
  IconAlreadyPresent
};

// Returns info about Chrome's presence in the Dock.
ChromeInDockStatus ChromeIsInTheDock();

#if defined(__OBJC__)

// Adds an icon to the Dock pointing to |installed_path| if one is not already
// present. |dmg_app_path| is the path to the install source. Its tile will be
// removed if present. If any changes are made to the Dock's configuration,
// the Dock process is restarted so that the changes become visible in the UI.
//
// Various heuristics are used to determine where to place the new icon
// relative to other items already present in the Dock:
//  - If installed_path is already in the Dock, no new tiles for this path
//    will be added.
//  - If dmg_app_path is present in the Dock, it will be removed. If
//    installed_path is not already present, the new tile referencing
//    installed_path will be placed where the dmg_app_path tile was. This
//    keeps the tile where a user expects it if they dragged the application
//    icon from a disk image into the Dock and then clicked on the new icon
//    in the Dock.
//  - The new tile will precede any application with the same name already
//    in the Dock.
//  - In an official build, a new tile for Google Chrome will be placed
//    immediately before the first existing tile for Google Chrome Canary,
//    and a new tile for Google Chrome Canary will be placed immediately after
//    the last existing tile for Google Chrome.
//  - The new tile will be placed immediately after the last tile for another
//    browser application already in the Dock.
//  - The new tile will be placed last in the Dock.
// For the purposes of these comparisons, applications are identified by the
// last component in their path. For example, any application named Safari.app
// will be treated as a browser. If the user renames an application on disk,
// it will alter the result. Looking up the bundle ID could be slightly more
// robust in the presence of such alterations, but it's not thought to be a
// large enough problem to warrant such lookups.
//
// The changes made to the Dock's configuration are the minimal changes
// necessary to cause the desired behavior. Although it's possible to set
// additional properties on the dock tile added to the Dock's plist, this
// is not done. Upon relaunch, Dock.app will determine the correct values for
// the properties it requires and add them to its configuration.
AddIconStatus AddIcon(NSString* installed_path, NSString* dmg_app_path);

#endif  // __OBJC__

}  // namespace dock

#endif  // CHROME_BROWSER_MAC_DOCK_H_
