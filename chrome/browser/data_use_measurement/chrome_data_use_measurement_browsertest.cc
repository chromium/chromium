// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_use_measurement/core/data_use_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace data_use_measurement {

class ChromeDataUseMeasurementBrowsertestBase : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory(
        GetChromeTestDataDir());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SimulateNetworkChange(network::mojom::ConnectionType type) {
    if (!content::IsInProcessNetworkService()) {
      mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
      content::GetNetworkService()->BindTestInterface(
          network_service_test.BindNewPipeAndPassReceiver());
      base::RunLoop run_loop;
      network_service_test->SimulateNetworkChange(type, run_loop.QuitClosure());
      run_loop.Run();
      return;
    }
    net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
        net::NetworkChangeNotifier::ConnectionType(type));
  }

  size_t GetCountEntriesUserInitiatedDataUsePrefs() const {
    return local_state()
               ->GetDictionary(
                   data_use_measurement::prefs::kDataUsedUserForeground)
               ->size() +
           local_state()
               ->GetDictionary(
                   data_use_measurement::prefs::kDataUsedUserBackground)
               ->size();
  }

  void RetryUntilUserInitiatedDataUsePrefHasEntry() {
    do {
      base::ThreadPoolInstance::Get()->FlushForTesting();
      base::RunLoop().RunUntilIdle();
    } while (GetCountEntriesUserInitiatedDataUsePrefs() == 0);
  }

  PrefService* local_state() const { return g_browser_process->local_state(); }

 private:
};

class ChromeDataUseMeasurementBrowsertest
    : public ChromeDataUseMeasurementBrowsertestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeDataUseMeasurementBrowsertestBase::SetUpCommandLine(command_line);
  }
};

// Flaky on Linux (and Linux MSAN): https://crbug.com/1141975.
#if defined(OS_LINUX)
#define MAYBE_DataUseTrackerPrefsUpdated DISABLED_DataUseTrackerPrefsUpdated
#else
#define MAYBE_DataUseTrackerPrefsUpdated DataUseTrackerPrefsUpdated
#endif

IN_PROC_BROWSER_TEST_F(ChromeDataUseMeasurementBrowsertest,
                       MAYBE_DataUseTrackerPrefsUpdated) {
  SimulateNetworkChange(network::mojom::ConnectionType::CONNECTION_3G);

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  RetryUntilUserInitiatedDataUsePrefHasEntry();

  EXPECT_EQ(1u, GetCountEntriesUserInitiatedDataUsePrefs());
}

}  // namespace data_use_measurement
