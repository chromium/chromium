// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/firefox_profile_lock.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "base/file_descriptor_posix.h"
#include "base/files/file_util.h"

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

void FirefoxProfileLock::Init() {
  lock_fd_ = base::kInvalidFd;
}

void FirefoxProfileLock::Lock() {
  if (HasAcquired())
    return;

  bool fcntl_lock = LockWithFcntl();
  if (!fcntl_lock) {
    return;
  } else if (!HasAcquired()) {
    old_lock_file_ = lock_file_.DirName().Append(kOldLockFileName);
    lock_fd_ = open(old_lock_file_.value().c_str(), O_CREAT | O_EXCL, 0644);
  }
}

void FirefoxProfileLock::Unlock() {
  if (!HasAcquired())
    return;
  close(lock_fd_);
  lock_fd_ = base::kInvalidFd;
  base::DeleteFile(old_lock_file_);
}

bool FirefoxProfileLock::HasAcquired() {
  return lock_fd_ > base::kInvalidFd;
}

// This function tries to lock Firefox profile using fcntl(). The return
// value of this function together with HasAcquired() tells the current status
// of lock.
// if return == false: Another process has lock to the profile.
// if return == true && HasAcquired() == true: successfully acquired the lock.
// if return == false && HasAcquired() == false: Failed to acquire lock due
// to some error (so that we can try alternate method of profile lock).
bool FirefoxProfileLock::LockWithFcntl() {
  lock_fd_ = open(lock_file_.value().c_str(), O_WRONLY | O_CREAT | O_TRUNC,
                  0666);
  if (lock_fd_ == base::kInvalidFd)
    return true;

  struct flock lock;
  lock.l_start = 0;
  lock.l_len = 0;
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  lock.l_pid = 0;

  struct flock testlock = lock;
  if (fcntl(lock_fd_, F_GETLK, &testlock) == -1) {
    close(lock_fd_);
    lock_fd_ = base::kInvalidFd;
    return true;
  } else if (fcntl(lock_fd_, F_SETLK, &lock) == -1) {
    close(lock_fd_);
    lock_fd_ = base::kInvalidFd;
    if (errno == EAGAIN || errno == EACCES)
      return false;
    else
      return true;
  } else {
    // We have the lock.
    return true;
  }
}
