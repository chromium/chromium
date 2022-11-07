// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/firefox_profile_lock.h"

#include "base/files/file_path.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"

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

// static
#if BUILDFLAG(IS_MAC)
const base::FilePath::CharType FirefoxProfileLock::kLockFileName[] =
    FILE_PATH_LITERAL(".parentlock");
const base::FilePath::CharType FirefoxProfileLock::kOldLockFileName[] =
    FILE_PATH_LITERAL("parent.lock");
#elif BUILDFLAG(IS_POSIX)
// http://www.google.com/codesearch/p?hl=en#e_ObwTAVPyo/profile/dirserviceprovider/src/nsProfileLock.cpp&l=433
const base::FilePath::CharType FirefoxProfileLock::kLockFileName[] =
    FILE_PATH_LITERAL(".parentlock");
const base::FilePath::CharType FirefoxProfileLock::kOldLockFileName[] =
    FILE_PATH_LITERAL("lock");
#else
const base::FilePath::CharType FirefoxProfileLock::kLockFileName[] =
    FILE_PATH_LITERAL("parent.lock");
#endif

FirefoxProfileLock::FirefoxProfileLock(const base::FilePath& path) {
  Init();
  lock_file_ = path.Append(kLockFileName);
  Lock();
}

FirefoxProfileLock::~FirefoxProfileLock() {
  // Because this destructor happens in first run on the profile import thread,
  // with no UI to jank, it's ok to allow deletion of the lock here.
  base::ScopedAllowBlocking allow_blocking;
  Unlock();
}
