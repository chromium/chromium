// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_guest_os.h"

#include "extensions/browser/extension_function.h"

namespace extensions {

ExtensionFunction::ResponseAction
FileManagerPrivateListMountableGuestsFunction::Run() {
  return RespondLater();
}

}  // namespace extensions
