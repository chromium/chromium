// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_FIREFOX_PROFILE_LOCK_H__
#define CHROME_BROWSER_IMPORTER_FIREFOX_PROFILE_LOCK_H__

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"

// Firefox is designed to allow only one application to access its
// profile at the same time.
// Reference:
//   http://kb.mozillazine.org/Profile_in_use
//
// This class is based on Firefox code in:
//   profile/dirserviceprovider/src/nsProfileLock.cpp
// The license block is:

/* ***** BEGIN LICENSE BLOCK *****
* Version: MPL 1.1/GPL 2.0/LGPL 2.1
*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with
* the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is mozilla.org code.
*
* The Initial Developer of the Original Code is
* Netscape Communications Corporation.
* Portions created by the Initial Developer are Copyright (C) 2002
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
*   Conrad Carlen <ccarlen@netscape.com>
*   Brendan Eich <brendan@mozilla.org>
*   Colin Blake <colin@theblakes.com>
*   Javier Pedemonte <pedemont@us.ibm.com>
*   Mats Palmgren <mats.palmgren@bredband.net>
*
* Alternatively, the contents of this file may be used under the terms of
* either the GNU General Public License Version 2 or later (the "GPL"), or
* the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
* in which case the provisions of the GPL or the LGPL are applicable instead
* of those above. If you wish to allow use of your version of this file only
* under the terms of either the GPL or the LGPL, and not to allow others to
* use your version of this file under the terms of the MPL, indicate your
* decision by deleting the provisions above and replace them with the notice
* and other provisions required by the GPL or the LGPL. If you do not delete
* the provisions above, a recipient may use your version of this file under
* the terms of any one of the MPL, the GPL or the LGPL.
*
* ***** END LICENSE BLOCK ***** */

class FirefoxProfileLock {
 public:
  explicit FirefoxProfileLock(const base::FilePath& path);

  FirefoxProfileLock(const FirefoxProfileLock&) = delete;
  FirefoxProfileLock& operator=(const FirefoxProfileLock&) = delete;

  ~FirefoxProfileLock();

  // Locks and releases the profile.
  void Lock();
  void Unlock();

  // Returns true if we lock the profile successfully.
  bool HasAcquired();

 private:
  FRIEND_TEST_ALL_PREFIXES(FirefoxProfileLockTest, ProfileLock);
  FRIEND_TEST_ALL_PREFIXES(FirefoxProfileLockTest, ProfileLockOrphaned);

  static const base::FilePath::CharType kLockFileName[];
  static const base::FilePath::CharType kOldLockFileName[];

  void Init();

  // Full path of the lock file in the profile folder.
  base::FilePath lock_file_;

  // The handle of the lock file.
#if BUILDFLAG(IS_WIN)
  HANDLE lock_handle_;
#elif BUILDFLAG(IS_POSIX)
  int lock_fd_;

  // On Posix systems Firefox apparently first tries to put a fcntl lock
  // on a file and if that fails, it does a regular exculsive open on another
  // file. This variable contains the location of this other file.
  base::FilePath old_lock_file_;

  // Method that tries to put a fcntl lock on file specified by |lock_file_|.
  // Returns false if lock is already held by another process. true in all
  // other cases.
  bool LockWithFcntl();
#endif
};

#endif  // CHROME_BROWSER_IMPORTER_FIREFOX_PROFILE_LOCK_H__
