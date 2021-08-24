// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service_factory.h"

#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/common_signals_decorator.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_decorator.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service_impl.h"

#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
#include "base/check.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/browser/browser_signals_decorator.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)

namespace enterprise_connectors {

std::unique_ptr<SignalsService> CreateSignalsService() {
  std::vector<std::unique_ptr<SignalsDecorator>> decorators;

  decorators.push_back(std::make_unique<CommonSignalsDecorator>());

#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
  policy::ChromeBrowserPolicyConnector* browser_policy_connector =
      g_browser_process->browser_policy_connector();
  DCHECK(browser_policy_connector);
  DCHECK(browser_policy_connector->machine_level_user_cloud_policy_manager());
  decorators.push_back(std::make_unique<BrowserSignalsDecorator>(
      policy::BrowserDMTokenStorage::Get(),
      browser_policy_connector->machine_level_user_cloud_policy_manager()
          ->store()));
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)

  return std::make_unique<SignalsServiceImpl>(std::move(decorators));
}

}  // namespace enterprise_connectors
