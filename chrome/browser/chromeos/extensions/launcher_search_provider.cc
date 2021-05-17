// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/launcher_search_provider.h"

#include <memory>
#include <utility>

#include "chrome/common/extensions/api/launcher_search_provider.h"
#include "content/public/browser/render_frame_host.h"

namespace extensions {

LauncherSearchProviderSetSearchResultsFunction::
    ~LauncherSearchProviderSetSearchResultsFunction() {}

ExtensionFunction::ResponseAction
LauncherSearchProviderSetSearchResultsFunction::Run() {
  return RespondNow(NoArguments());
}

}  // namespace extensions
