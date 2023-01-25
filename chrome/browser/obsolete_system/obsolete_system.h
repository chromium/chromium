// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OBSOLETE_SYSTEM_OBSOLETE_SYSTEM_H_
#define CHROME_BROWSER_OBSOLETE_SYSTEM_OBSOLETE_SYSTEM_H_

#include <string>

namespace ObsoleteSystem {

// Returns true if the system is already considered obsolete, or if it'll be
// considered obsolete soon. Used to control whether to show messaging about
// deprecation within the app.
bool IsObsoleteNowOrSoon();

// Returns a localized string informing users that their system will either soon
// be unsupported by future versions of the application, or that they are
// already using the last version of the application that supports their system.
// Do not use the returned string unless IsObsoleteNowOrSoon() returns true.
std::u16string LocalizedObsoleteString();

// Returns true if this is the final release milestone. This is only valid
// when IsObsoleteNowOrSoon() returns true.
//
// If true, about:help will stop showing "Checking for updates... Chrome is up
// to date", and users can no longer manually check for updates by refreshing
// about:help. This is typically done when the last milestone supporting an
// obsolete OS version is reached, to make it clear that Chrome will no longer
// check for major updates. Note that even if the implementation returns true
// when the last supported milestone has been reached, users will continue to
// get any released minor updates for that milestone despite the lack of a
// "Checking for updates..." message on about:help.
bool IsEndOfTheLine();

// A help URL to explain the deprecation. Do not use the returned string
// unless IsObsoleteNowOrSoon() returns true.
const char* GetLinkURL();

}  // namespace ObsoleteSystem

#endif  // CHROME_BROWSER_OBSOLETE_SYSTEM_OBSOLETE_SYSTEM_H_
