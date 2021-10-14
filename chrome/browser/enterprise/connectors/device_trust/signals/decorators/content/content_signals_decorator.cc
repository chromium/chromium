// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/content/content_signals_decorator.h"

#include "base/callback.h"
#include "chrome/browser/enterprise/signals/signals_utils.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "content/public/browser/site_isolation_policy.h"

namespace enterprise_connectors {

ContentSignalsDecorator::ContentSignalsDecorator(
    PolicyBlocklistService* policy_blocklist_service)
    : policy_blocklist_service_(policy_blocklist_service) {
  DCHECK(policy_blocklist_service_);
}

ContentSignalsDecorator::~ContentSignalsDecorator() = default;

void ContentSignalsDecorator::Decorate(SignalsType& signals,
                                       base::OnceClosure done_closure) {
  signals.set_remote_desktop_available(
      enterprise_signals::utils::GetChromeRemoteDesktopAppBlocked(
          policy_blocklist_service_));
  signals.set_site_isolation_enabled(
      content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());

  std::move(done_closure).Run();
}

}  // namespace enterprise_connectors
