// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"
#include "chrome/browser/devtools/device/tcp_device_provider.h"
#include "chrome/browser/devtools/remote_debugging_server.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {
const char kPortForwardingTestPage[] = "/devtools/port_forwarding/main.html";

const int kDefaultDebuggingPort = 9223;
const int kAlternativeDebuggingPort = 9224;

}

class PortForwardingTest: public InProcessBrowserTest {
  virtual int GetRemoteDebuggingPort() {
    return kDefaultDebuggingPort;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kRemoteDebuggingPort,
        base::NumberToString(GetRemoteDebuggingPort()));
  }

 protected:
  class Listener : public DevToolsAndroidBridge::PortForwardingListener {
   public:
    explicit Listener(Profile* profile)
        : profile_(profile), skip_empty_devices_(true) {
      DevToolsAndroidBridge::Factory::GetForProfile(profile_)
          ->AddPortForwardingListener(this);
    }

    ~Listener() override {
      DevToolsAndroidBridge::Factory::GetForProfile(profile_)->
          RemovePortForwardingListener(this);
    }

    void PortStatusChanged(const ForwardingStatus& status) override {
      if (status.empty() && skip_empty_devices_)
        return;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, loop_->QuitWhenIdleClosure());
    }

    void set_skip_empty_devices(bool skip_empty_devices) {
      skip_empty_devices_ = skip_empty_devices;
    }

    void set_run_loop(base::RunLoop* loop) { loop_ = loop; }

   private:
    raw_ptr<Profile> profile_;
    bool skip_empty_devices_;
    raw_ptr<base::RunLoop> loop_;
  };
};

IN_PROC_BROWSER_TEST_F(PortForwardingTest, LoadPageWithStyleAnsScript) {
  Profile* profile = browser()->profile();
  AndroidDeviceManager::DeviceProviders device_providers;

  device_providers.push_back(
      TCPDeviceProvider::CreateForLocalhost(kDefaultDebuggingPort));
  DevToolsAndroidBridge::Factory::GetForProfile(profile)->
      set_device_providers_for_test(device_providers);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL original_url = embedded_test_server()->GetURL(kPortForwardingTestPage);

  std::string forwarding_port("8000");
  GURL forwarding_url(original_url.scheme() + "://" +
      original_url.host() + ":" + forwarding_port + original_url.path());

  PrefService* prefs = profile->GetPrefs();
  prefs->SetBoolean(prefs::kDevToolsPortForwardingEnabled, true);

  base::Value::Dict config;
  config.Set(forwarding_port, original_url.host() + ":" + original_url.port());
  prefs->SetDict(prefs::kDevToolsPortForwardingConfig, std::move(config));
  base::RunLoop loop1;
  Listener wait_for_port_forwarding(profile);
  wait_for_port_forwarding.set_run_loop(&loop1);
  loop1.Run();

  RemoteDebuggingServer::EnableTetheringForDebug();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), forwarding_url));

  content::WebContents* wc = browser()->tab_strip_model()->GetWebContentsAt(0);
  ASSERT_TRUE(content::WaitForLoadStop(wc));

  ASSERT_EQ("Port forwarding test", content::EvalJs(wc, "document.title"))
      << "Document has not loaded.";

  ASSERT_EQ("content", content::EvalJs(wc, "getBodyTextContent()"))
      << "Javascript has not loaded.";

  ASSERT_EQ("100px", content::EvalJs(wc, "getBodyMarginLeft()"))
      << "CSS has not loaded.";

  // Test that disabling port forwarding is handled normally.
  base::RunLoop loop2;
  wait_for_port_forwarding.set_skip_empty_devices(false);
  wait_for_port_forwarding.set_run_loop(&loop2);
  prefs->SetBoolean(prefs::kDevToolsPortForwardingEnabled, false);

  loop2.Run();
}

class PortForwardingDisconnectTest : public PortForwardingTest {
  int GetRemoteDebuggingPort() override {
    return kAlternativeDebuggingPort;
  }
};

IN_PROC_BROWSER_TEST_F(PortForwardingDisconnectTest, DisconnectOnRelease) {
  Profile* profile = browser()->profile();

  AndroidDeviceManager::DeviceProviders device_providers;

  scoped_refptr<TCPDeviceProvider> self_provider(
      TCPDeviceProvider::CreateForLocalhost(kAlternativeDebuggingPort));
  device_providers.push_back(self_provider);

  DevToolsAndroidBridge::Factory::GetForProfile(profile)->
      set_device_providers_for_test(device_providers);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL original_url = embedded_test_server()->GetURL(kPortForwardingTestPage);

  std::string forwarding_port("8000");
  GURL forwarding_url(original_url.scheme() + "://" +
      original_url.host() + ":" + forwarding_port + original_url.path());

  PrefService* prefs = profile->GetPrefs();
  prefs->SetBoolean(prefs::kDevToolsPortForwardingEnabled, true);

  base::Value::Dict config;
  config.Set(forwarding_port, original_url.host() + ":" + original_url.port());
  prefs->SetDict(prefs::kDevToolsPortForwardingConfig, std::move(config));

  base::RunLoop run_loop;

  auto wait_for_port_forwarding = std::make_unique<Listener>(profile);
  wait_for_port_forwarding->set_run_loop(&run_loop);

  self_provider->set_release_callback_for_test(base::BindOnce(
      base::IgnoreResult(&base::SingleThreadTaskRunner::PostTask),
      base::SingleThreadTaskRunner::GetCurrentDefault(), FROM_HERE,
      run_loop.QuitWhenIdleClosure()));
  wait_for_port_forwarding.reset();

  run_loop.Run();
}
