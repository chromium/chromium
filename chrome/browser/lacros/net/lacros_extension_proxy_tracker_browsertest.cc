// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/net/lacros_extension_proxy_tracker.h"

#include "base/files/file_util.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chromeos/crosapi/mojom/network_settings_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace {
// Fakes the NetworkSettingsService in Ash-Chrome so we can send proxy updates
// from the tests via the mojo API.
class FakeNetworkSettingsService
    : public crosapi::mojom::NetworkSettingsService {
 public:
  FakeNetworkSettingsService() = default;
  FakeNetworkSettingsService(const FakeNetworkSettingsService&) = delete;
  FakeNetworkSettingsService& operator=(const FakeNetworkSettingsService&) =
      delete;
  ~FakeNetworkSettingsService() override = default;

  // crosapi::mojom::AshNetworkService:
  void AddNetworkSettingsObserver(
      mojo::PendingRemote<crosapi::mojom::NetworkSettingsObserver> observer)
      override {}

  void SetExtensionProxy(crosapi::mojom::ProxyConfigPtr proxy_config) override {
    cached_proxy_config_ = std::move(proxy_config);
    if (set_quit_closure_) {
      std::move(set_quit_closure_).Run();
    }
  }

  void ClearExtensionProxy() override {
    cached_proxy_config_.reset();
    if (clear_quit_closure_) {
      std::move(clear_quit_closure_).Run();
    }
  }

  void SetQuitClosure(base::OnceClosure quit_closure) {
    set_quit_closure_ = std::move(quit_closure);
  }

  void ClearQuitClosure(base::OnceClosure quit_closure) {
    clear_quit_closure_ = std::move(quit_closure);
  }

  crosapi::mojom::ProxyConfig* cached_proxy_config() const {
    return cached_proxy_config_ ? cached_proxy_config_.get() : nullptr;
  }

 private:
  crosapi::mojom::ProxyConfigPtr cached_proxy_config_ = nullptr;
  base::OnceClosure set_quit_closure_, clear_quit_closure_;
};

}  // namespace

namespace lacros {
namespace net {

class LacrosExtensionProxyTrackerTest
    : public extensions::ExtensionBrowserTest {
 public:
  LacrosExtensionProxyTrackerTest() = default;
  ~LacrosExtensionProxyTrackerTest() override = default;

  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
    if (!IsServiceAvailable())
      return;
    // Replace the production network settings service with a fake for testing.
    mojo::Remote<crosapi::mojom::NetworkSettingsService>& remote =
        chromeos::LacrosService::Get()
            ->GetRemote<crosapi::mojom::NetworkSettingsService>();
    remote.reset();
    receiver_.Bind(remote.BindNewPipeAndPassReceiver());
  }

 protected:
  // Returns false if the network settings service interface is not available or
  // doesn't support extension set proxies on this version of Ash-Chrome.
  bool IsServiceAvailable() {
    auto* lacros_service = chromeos::LacrosService::Get();
    if (!lacros_service)
      return false;
    return lacros_service
               ->IsAvailable<crosapi::mojom::NetworkSettingsService>() &&
           LacrosExtensionProxyTracker::AshVersionSupportsExtensionSetProxies();
  }

  FakeNetworkSettingsService service_;
  mojo::Receiver<crosapi::mojom::NetworkSettingsService> receiver_{&service_};
};

// Test that verifies that proxies set via an extension in Lacros are propagated
// to Ash. This test also verifies that uninstalling the extension results in a
// mojo request to Ash to clear the extension set proxy.
IN_PROC_BROWSER_TEST_F(LacrosExtensionProxyTrackerTest, ExtensionSetProxy) {
  if (!IsServiceAvailable())
    return;

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath extension_path = base::MakeAbsoluteFilePath(
      test_data_dir_.AppendASCII("api_test/proxy/pac"));
  if (extension_path.empty())
    return;

  EXPECT_FALSE(service_.cached_proxy_config());
  std::string extension_id;
  {
    base::RunLoop run_loop;
    service_.SetQuitClosure(run_loop.QuitClosure());
    // The test extension code is hosted at
    // //chrome/test/data/extensions/api_test/proxy/pac/.
    extension_id = LoadExtension(extension_path)->id();
    // Wait for the AshNetworkSettingsService to receive a mojo request to set
    // the extension proxy.
    run_loop.Run();
  }
  ASSERT_TRUE(service_.cached_proxy_config());
  ASSERT_TRUE(service_.cached_proxy_config()->proxy_settings->is_pac());
  EXPECT_EQ(service_.cached_proxy_config()->proxy_settings->get_pac()->pac_url,
            GURL("http://wpad/windows.pac"));

  {
    base::RunLoop run_loop;
    service_.ClearQuitClosure(run_loop.QuitClosure());
    UninstallExtension(extension_id);
    // Wait for the AshNetworkSettingsService to receive a mojo request to clear
    // the extension proxy.
    run_loop.Run();
  }
  EXPECT_FALSE(service_.cached_proxy_config());
}

}  // namespace net
}  // namespace lacros
