// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/net/lacros_extension_proxy_tracker.h"

#include "base/files/file_util.h"
#include "base/test/test_future.h"
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
  FakeNetworkSettingsService() {
    // Replace the production network settings service with a fake for testing.
    mojo::Remote<crosapi::mojom::NetworkSettingsService>& remote =
        chromeos::LacrosService::Get()
            ->GetRemote<crosapi::mojom::NetworkSettingsService>();
    remote.reset();
    receiver_.Bind(remote.BindNewPipeAndPassReceiver());
  }
  FakeNetworkSettingsService(const FakeNetworkSettingsService&) = delete;
  FakeNetworkSettingsService& operator=(const FakeNetworkSettingsService&) =
      delete;
  ~FakeNetworkSettingsService() override = default;

  // crosapi::mojom::AshNetworkService:
  void AddNetworkSettingsObserver(
      mojo::PendingRemote<crosapi::mojom::NetworkSettingsObserver> observer)
      override {}

  void SetExtensionProxy(crosapi::mojom::ProxyConfigPtr proxy_config) override {
    // When enabling an extension, the same "ExtensionReady" ExtensionRegistry
    // event is triggered twice resulting in the same extension metadata being
    // sent twice. We only care for the latest update.
    set_extension_proxy_future_.Clear();
    set_extension_proxy_future_.SetValue(std::move(proxy_config));
  }
  void ClearExtensionProxy() override {
    clear_extension_proxy_future_.Clear();
    clear_extension_proxy_future_.SetValue(true);
  }
  void SetExtensionControllingProxyMetadata(
      crosapi::mojom::ExtensionControllingProxyPtr extension) override {
    // When enabling an extension, the same "ExtensionReady" ExtensionRegistry
    // event is triggered twice resulting in the same extension metadata being
    // sent twice. We only care for the latest update.
    set_extension_metadata_future_.Clear();
    set_extension_metadata_future_.SetValue(std::move(extension));
  }
  void ClearExtensionControllingProxyMetadata() override {
    clear_extension_metadata_future_.Clear();
    clear_extension_metadata_future_.SetValue(true);
  }

  void IsAlwaysOnVpnPreConnectUrlAllowlistEnforced(
      IsAlwaysOnVpnPreConnectUrlAllowlistEnforcedCallback callback) override {}

  crosapi::mojom::ProxyConfigPtr WaitSetExtensionProxy() {
    return set_extension_proxy_future_.Take();
  }
  bool WaitClearExtensionProxy() {
    return clear_extension_proxy_future_.Take();
  }
  crosapi::mojom::ExtensionControllingProxyPtr
  WaitSetExtensionControllingProxyMetadata() {
    return set_extension_metadata_future_.Take();
  }
  bool WaitClearExtensionControllingProxyMetadata() {
    return clear_extension_metadata_future_.Take();
  }

 private:
  base::test::TestFuture<crosapi::mojom::ProxyConfigPtr>
      set_extension_proxy_future_;
  base::test::TestFuture<bool> clear_extension_proxy_future_;
  base::test::TestFuture<crosapi::mojom::ExtensionControllingProxyPtr>
      set_extension_metadata_future_;
  base::test::TestFuture<bool> clear_extension_metadata_future_;
  mojo::Receiver<crosapi::mojom::NetworkSettingsService> receiver_{this};
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
    service_ = std::make_unique<FakeNetworkSettingsService>();
  }

 protected:
  // Returns false if the network settings service interface is not available or
  // doesn't support extension set proxies on this version of Ash-Chrome.
  bool IsExtensionMetadataSupported() {
    auto* lacros_service = chromeos::LacrosService::Get();
    if (!lacros_service) {
      return false;
    }
    return lacros_service
               ->IsAvailable<crosapi::mojom::NetworkSettingsService>() &&
           LacrosExtensionProxyTracker::AshVersionSupportsExtensionMetadata();
  }

  void VerifyExtensionMetadataSent(const std::string& extension_id,
                                   const std::string& extension_name) {
    if (IsExtensionMetadataSupported()) {
      auto extension = service_->WaitSetExtensionControllingProxyMetadata();
      ASSERT_TRUE(extension);
      EXPECT_EQ(extension->id, extension_id);
      EXPECT_EQ(extension->name, extension_name);
      return;
    }
    auto proxy_config = service_->WaitSetExtensionProxy();
    ASSERT_TRUE(proxy_config);
    ASSERT_TRUE(proxy_config->extension);
    EXPECT_EQ(proxy_config->extension->id, extension_id);
    EXPECT_EQ(proxy_config->extension->name, extension_name);
    // Verify that the proxy config set by the extension hosted at
    // //chrome/test/data/extensions/api_test/proxy/pac is sent to Ash.
    ASSERT_TRUE(proxy_config->proxy_settings->is_pac());
    EXPECT_EQ(proxy_config->proxy_settings->get_pac()->pac_url,
              GURL("http://wpad/windows.pac"));
  }

  void VerifyExtensionClearRequestSent() {
    if (IsExtensionMetadataSupported()) {
      EXPECT_TRUE(service_->WaitClearExtensionControllingProxyMetadata());
      return;
    }
    EXPECT_TRUE(service_->WaitClearExtensionProxy());
  }
  std::unique_ptr<FakeNetworkSettingsService> service_;
};

// Test that verifies that proxies set via an extension in Lacros are propagated
// to Ash. This test also verifies that uninstalling the extension results in a
// mojo request to Ash to clear the extension set proxy.
IN_PROC_BROWSER_TEST_F(LacrosExtensionProxyTrackerTest, ExtensionSetProxy) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath extension_path = base::MakeAbsoluteFilePath(
      test_data_dir_.AppendASCII("api_test/proxy/pac"));
  if (extension_path.empty()) {
    return;
  }

  std::string extension_id;
  // The test extension code is hosted at
  // //chrome/test/data/extensions/api_test/proxy/pac/.
  extension_id = LoadExtension(extension_path)->id();

  VerifyExtensionMetadataSent(extension_id, "chrome.proxy");

  UninstallExtension(extension_id);

  VerifyExtensionClearRequestSent();
}

// Test that the extension metadata is sent when the extension is loaded, by
// reacting to ExtensionRegistry events. Specifically, this test checks that the
// mextension data is sent after disabling and re-enabling the extension. In
// this case, the proxy pref is available before the extensio is loaded.
IN_PROC_BROWSER_TEST_F(LacrosExtensionProxyTrackerTest,
                       SendUpdatesOnExtensionLoaded) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath extension_path = base::MakeAbsoluteFilePath(
      test_data_dir_.AppendASCII("api_test/proxy/pac"));
  if (extension_path.empty()) {
    return;
  }
  std::string extension_id;
  // The test extension code is hosted at
  // //chrome/test/data/extensions/api_test/proxy/pac/.
  extension_id = LoadExtension(extension_path)->id();
  VerifyExtensionMetadataSent(extension_id, "chrome.proxy");

  DisableExtension(extension_id);
  VerifyExtensionClearRequestSent();

  EnableExtension(extension_id);
  VerifyExtensionMetadataSent(extension_id, "chrome.proxy");
}

}  // namespace net
}  // namespace lacros
