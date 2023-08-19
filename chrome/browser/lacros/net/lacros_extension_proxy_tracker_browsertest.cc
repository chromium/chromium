// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/net/lacros_extension_proxy_tracker.h"

#include "base/files/file_util.h"
#include "base/test/repeating_test_future.h"
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
    set_extension_proxy_future_.AddValue(std::move(proxy_config));
  }
  void ClearExtensionProxy() override {
    clear_extension_proxy_future_.AddValue(true);
  }
  void SetExtensionControllingProxyMetadata(
      crosapi::mojom::ExtensionControllingProxyPtr extension) override {
    set_extension_metadata_future_.AddValue(std::move(extension));
  }
  void ClearExtensionControllingProxyMetadata() override {
    clear_extension_metadata_future_.AddValue(true);
  }

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
  base::test::RepeatingTestFuture<crosapi::mojom::ProxyConfigPtr>
      set_extension_proxy_future_;
  // TODO(crbug/1379290): Replace with `RepeatingTestFuture<void>`
  base::test::RepeatingTestFuture<bool> clear_extension_proxy_future_;
  base::test::RepeatingTestFuture<crosapi::mojom::ExtensionControllingProxyPtr>
      set_extension_metadata_future_;
  base::test::RepeatingTestFuture<bool> clear_extension_metadata_future_;
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

  if (IsExtensionMetadataSupported()) {
    auto extension = service_->WaitSetExtensionControllingProxyMetadata();
    ASSERT_TRUE(extension);
    EXPECT_EQ(extension->id, extension_id);
    EXPECT_EQ(extension->name, "chrome.proxy");

  } else {
    auto proxy_config = service_->WaitSetExtensionProxy();
    ASSERT_TRUE(proxy_config);
    ASSERT_TRUE(proxy_config->proxy_settings->is_pac());
    EXPECT_EQ(proxy_config->proxy_settings->get_pac()->pac_url,
              GURL("http://wpad/windows.pac"));
  }
  UninstallExtension(extension_id);

  if (IsExtensionMetadataSupported()) {
    EXPECT_TRUE(service_->WaitClearExtensionControllingProxyMetadata());
  } else {
    EXPECT_TRUE(service_->WaitClearExtensionProxy());
  }
}

}  // namespace net
}  // namespace lacros
