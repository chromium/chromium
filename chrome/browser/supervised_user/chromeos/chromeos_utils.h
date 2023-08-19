// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_CHROMEOS_UTILS_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_CHROMEOS_UTILS_H_

namespace crosapi::mojom {
class ParentAccess;
}  // namespace crosapi::mojom

namespace supervised_user {

// Returns parent access API handle.
crosapi::mojom::ParentAccess* GetParentAccessApi();

}  // namespace supervised_user

#endif  // CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_CHROMEOS_UTILS_H_
