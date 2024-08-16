// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/crash_reporting_context.h"

#include "base/json/json_reader.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/reporting/browser_crash_event_router.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"

namespace enterprise_connectors {

#if !BUILDFLAG(IS_CHROMEOS)

using CrashReportingContextTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(CrashReportingContextTest, OnCloudReportingLaunched) {
  ::policy::SetDMTokenForTesting(
      ::policy::DMToken::CreateValidToken("FAKE_DM_TOKEN"));

  auto* profile = browser()->profile();
  profile->GetPrefs()->SetInteger(kOnSecurityEventScopePref,
                                  policy::POLICY_SCOPE_MACHINE);
  constexpr char kConnectorsPrefValue[] = R"([
    {
      "service_provider": "google"
    }
  ])";
  profile->GetPrefs()->Set(kOnSecurityEventPref,
                           *base::JSONReader::Read(kConnectorsPrefValue));

  BrowserCrashEventRouter router(profile);

  CrashReportingContext* crash_reporting_context =
      CrashReportingContext::GetInstance();

  // This should not crash. See https://crbug.com/1441715.
  crash_reporting_context->OnCloudReportingLaunched(nullptr);
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace enterprise_connectors
