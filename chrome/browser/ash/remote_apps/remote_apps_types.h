// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_REMOTE_APPS_REMOTE_APPS_TYPES_H_
#define CHROME_BROWSER_ASH_REMOTE_APPS_REMOTE_APPS_TYPES_H_

namespace chromeos {

enum class RemoteAppsError {
  kNone = 0,
  kAppIdDoesNotExist,
  kFolderIdDoesNotExist,
  // Manager has not been initialized.
  kNotReady,
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_REMOTE_APPS_REMOTE_APPS_TYPES_H_
