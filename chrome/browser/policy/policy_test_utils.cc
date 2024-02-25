// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_test_utils.h"

#include <optional>
#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/policy_constants.h"
#include "components/safe_search_api/safe_search_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"

using content::BrowserThread;

namespace policy {

void GetTestDataDirectory(base::FilePath* test_data_directory) {
  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_TEST_DATA, test_data_directory));
}

PolicyTest::PolicyTest() = default;

PolicyTest::~PolicyTest() = default;

void PolicyTest::SetUpInProcessBrowserTestFixture() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch("noerrdialogs");
  provider_.SetDefaultReturns(true /* is_initialization_complete_return */,
                              true /* is_first_policy_load_complete_return */);
  BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
}

void PolicyTest::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");
}

void PolicyTest::UpdateProviderPolicy(const PolicyMap& policy) {
  PolicyMap policy_with_defaults = policy.Clone();
#if BUILDFLAG(IS_CHROMEOS)
  SetEnterpriseUsersDefaults(&policy_with_defaults);
#endif
  provider_.UpdateChromePolicy(policy_with_defaults);
}

// static
void PolicyTest::SetPolicy(PolicyMap* policies,
                           const char* key,
                           std::optional<base::Value> value) {
  policies->Set(key, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                POLICY_SOURCE_CLOUD, std::move(value), nullptr);
}

// static
bool PolicyTest::FetchSubresource(content::WebContents* web_contents,
                                  const GURL& url) {
  std::string script(
      "var xhr = new XMLHttpRequest();"
      "xhr.open('GET', '");
  script += url.spec() +
            "', true);"
            "new Promise(resolve => {"
            "  xhr.onload = function (e) {"
            "    if (xhr.readyState === 4) {"
            "      resolve(xhr.status === 200);"
            "    }"
            "  };"
            "  xhr.onerror = function () {"
            "    resolve(false);"
            "  };"
            "  xhr.send(null)"
            "});";
  return content::EvalJs(web_contents, script).ExtractBool();
}

void PolicyTest::FlushBlocklistPolicy() {
  // Updates of the URLBlocklist are done on IO, after building the blocklist
  // on the blocking pool, which is initiated from IO.
  content::RunAllPendingInMessageLoop(BrowserThread::IO);
  content::RunAllTasksUntilIdle();
  content::RunAllPendingInMessageLoop(BrowserThread::IO);
}

PolicyTestAppTerminationObserver::PolicyTestAppTerminationObserver() {
  terminating_subscription_ = browser_shutdown::AddAppTerminatingCallback(
      base::BindOnce(&PolicyTestAppTerminationObserver::OnAppTerminating,
                     base::Unretained(this)));
}

PolicyTestAppTerminationObserver::~PolicyTestAppTerminationObserver() = default;

}  // namespace policy
