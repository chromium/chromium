// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_provider_change_relaunch.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/no_destructor.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_provider_change.h"
#include "chrome/browser/lifetime/application_lifetime.h"

namespace glic {

namespace {

base::RepeatingClosure& RestartCallbackForTesting() {
  static base::NoDestructor<base::RepeatingClosure> callback;
  return *callback;
}

}  // namespace

void MaybeRelaunchForProviderChange(Profile* profile) {
  if (GlicEnabling::GetPendingProviderChange(profile) ==
      GlicProviderChange::kNone) {
    return;
  }

  base::RecordAction(
      base::UserMetricsAction("Glic.ProviderChange.RelaunchClicked"));
  if (RestartCallbackForTesting()) {
    RestartCallbackForTesting().Run();
    return;
  }
  // TODO(b/578040846): Warn about open incognito/isolated windows before
  // relaunching (see BrowserLifetimeHandler).
  chrome::AttemptRestart();
}

base::AutoReset<base::RepeatingClosure> SetRestartCallbackForTesting(
    base::RepeatingClosure callback) {
  return base::AutoReset<base::RepeatingClosure>(&RestartCallbackForTesting(),
                                                 std::move(callback));
}

}  // namespace glic
