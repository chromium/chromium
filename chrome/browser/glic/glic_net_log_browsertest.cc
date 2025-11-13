// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "net/log/net_log_entry.h"
#include "net/log/net_log_event_type.h"
#include "net/log/test_net_log.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

namespace {

const char kTestGlicURL[] = "about:blank?main-page";
const char kTestGlicFreURL[] = "about:blank?fre-page";

}  // namespace

class GlicNetLogBrowserTest : public InProcessBrowserTest {
 public:
  GlicNetLogBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Load blank page in glic guest view
    command_line->AppendSwitchASCII(::switches::kGlicGuestURL, kTestGlicURL);
    command_line->AppendSwitchASCII(::switches::kGlicFreURL, kTestGlicFreURL);
  }

  net::RecordingNetLogObserver& net_log_observer() { return net_log_observer_; }

 private:
  GlicTestEnvironment glic_test_env_{
      {.fre_status = glic::prefs::FreStatus::kNotStarted}};
  net::RecordingNetLogObserver net_log_observer_;
};

// Tests that opening the UI logs a request to the Glic FRE.
IN_PROC_BROWSER_TEST_F(GlicNetLogBrowserTest, LogGlicFreRequestOnOpenUI) {
  Profile* profile = browser()->profile();

  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile));

  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
  glic_service->OpenFreDialogInNewTab(
      browser(), mojom::InvocationSource::kTopChromeButton);

  std::vector<net::NetLogEntry> entries = net_log_observer().GetEntries();
  auto it = std::ranges::find_if(entries, [&](const auto& entry) {
    if (entry.source.type != net::NetLogSourceType::URL_REQUEST ||
        entry.type != net::NetLogEventType::REQUEST_ALIVE) {
      return false;
    }
    std::optional<int> traffic_annotation =
        entry.params.FindInt("traffic_annotation");
    return traffic_annotation.has_value() &&
           traffic_annotation.value() ==
               net::internal::ComputeAnnotationHash("glic_fre_web_ui");
  });

  ASSERT_NE(it, entries.end())
      << "NetLog did not contain URL_REQUEST_START_JOB for Glic FRE WeUI";
  EXPECT_EQ(true, it->params.FindBool("dummy_request"));
  const std::string* url = it->params.FindString("url");
  ASSERT_TRUE(url);
  EXPECT_THAT(*url, testing::StartsWith(kTestGlicFreURL));
}

// Tests that opening the UI logs a request to the Glic main page.
IN_PROC_BROWSER_TEST_F(GlicNetLogBrowserTest, LogGlicRequestOnOpenUI) {
  Profile* profile = browser()->profile();

  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile));
  ASSERT_FALSE(GlicEnabling::IsReadyForProfile(profile));
  SetFRECompletion(profile, prefs::FreStatus::kCompleted);
  ASSERT_TRUE(GlicEnabling::IsReadyForProfile(profile));

  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
  glic_service->ToggleUI(nullptr, false, mojom::InvocationSource::kOsHotkey);

  std::vector<net::NetLogEntry> entries = net_log_observer().GetEntries();
  auto it = std::ranges::find_if(entries, [&](const auto& entry) {
    if (entry.source.type != net::NetLogSourceType::URL_REQUEST ||
        entry.type != net::NetLogEventType::REQUEST_ALIVE) {
      return false;
    }
    std::optional<int> traffic_annotation =
        entry.params.FindInt("traffic_annotation");
    return traffic_annotation.has_value() &&
           traffic_annotation.value() ==
               net::internal::ComputeAnnotationHash("glic_web_ui");
  });

  ASSERT_NE(it, entries.end())
      << "NetLog did not contain URL_REQUEST_START_JOB for Glic WebUI";
  EXPECT_EQ(true, it->params.FindBool("dummy_request"));
  const std::string* url = it->params.FindString("url");
  ASSERT_TRUE(url);
  EXPECT_THAT(*url, testing::StartsWith(kTestGlicURL));
}

}  // namespace glic
