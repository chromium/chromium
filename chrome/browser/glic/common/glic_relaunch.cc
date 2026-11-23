// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/glic_relaunch.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "chrome/browser/lifetime/application_lifetime.h"

namespace glic {

namespace {

base::RepeatingClosure& GetRestartCallbackOverride() {
  static base::NoDestructor<base::RepeatingClosure> callback;
  return *callback;
}

}  // namespace

void RestartBrowserNow() {
  if (GetRestartCallbackOverride()) {
    GetRestartCallbackOverride().Run();
    return;
  }
  // TODO(crbug.com/578040846): Warn about open incognito/isolated windows
  // before relaunching (see BrowserLifetimeHandler).
  chrome::AttemptRestart();
}

base::AutoReset<base::RepeatingClosure> SetRestartCallbackForTesting(
    base::RepeatingClosure callback) {
  return base::AutoReset<base::RepeatingClosure>(&GetRestartCallbackOverride(),
                                                 std::move(callback));
}

}  // namespace glic
