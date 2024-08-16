// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/net/network_settings_observer.h"

#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/network_settings_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

namespace {

// Fakes the NetworkSettingsService in Ash-Chrome so we can send updates
// from the tests via the mojo API.
class FakeNetworkSettingsService
    : public crosapi::mojom::NetworkSettingsService {
 public:
  FakeNetworkSettingsService() = default;
  FakeNetworkSettingsService(const FakeNetworkSettingsService&) = delete;
  FakeNetworkSettingsService& operator=(const FakeNetworkSettingsService&) =
      delete;
  ~FakeNetworkSettingsService() override = default;

  bool HasObservers() { return !observers_.empty(); }

  // crosapi::mojom::AshNetworService:
  void AddNetworkSettingsObserver(
      mojo::PendingRemote<crosapi::mojom::NetworkSettingsObserver> observer)
      override {
    mojo::Remote<crosapi::mojom::NetworkSettingsObserver> remote(
        std::move(observer));
    observers_.Add(std::move(remote));

    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  void IsAlwaysOnVpnPreConnectUrlAllowlistEnforced(
      IsAlwaysOnVpnPreConnectUrlAllowlistEnforcedCallback callback) override {
    std::move(callback).Run(alwayson_vpn_pre_connect_url_allowlist_enforced_);
  }

  void SetAlwaysOnVpnPreConnectUrlAllowlistEnforced(bool enforced) {
    alwayson_vpn_pre_connect_url_allowlist_enforced_ = enforced;

    for (const auto& obs : observers_) {
      obs->OnAlwaysOnVpnPreConnectUrlAllowlistEnforcedChanged(enforced);
    }
  }

  void SetExtensionProxy(crosapi::mojom::ProxyConfigPtr proxy_config) override {
    NOTREACHED();
  }
  void ClearExtensionProxy() override { NOTREACHED(); }
  void SetExtensionControllingProxyMetadata(
      crosapi::mojom::ExtensionControllingProxyPtr extension) override {
    NOTREACHED();
  }
  void ClearExtensionControllingProxyMetadata() override { NOTREACHED(); }

  void SetQuitClosure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

 private:
  bool alwayson_vpn_pre_connect_url_allowlist_enforced_ = false;
  mojo::RemoteSet<crosapi::mojom::NetworkSettingsObserver> observers_;
  base::OnceClosure quit_closure_;
};
}  // namespace

class NetworkSettingsObserverTest : public InProcessBrowserTest {
 protected:
  NetworkSettingsObserverTest() = default;

  NetworkSettingsObserverTest(const NetworkSettingsObserverTest&) = delete;
  NetworkSettingsObserverTest& operator=(const NetworkSettingsObserverTest&) =
      delete;
  ~NetworkSettingsObserverTest() override = default;

  bool IsServiceAvailable() {
    auto* lacros_service = chromeos::LacrosService::Get();
    if (!lacros_service ||
        !lacros_service
             ->IsAvailable<crosapi::mojom::NetworkSettingsService>()) {
      return false;
    }

    // Check if Ash is too old to support
    // NetworkSettingsObserver.
    int version =
        lacros_service
            ->GetInterfaceVersion<crosapi::mojom::NetworkSettingsService>();
    int min_required_version = static_cast<int>(
        crosapi::mojom::NetworkSettingsService::MethodMinVersions::
            kIsAlwaysOnVpnPreConnectUrlAllowlistEnforcedMinVersion);
    return version > min_required_version;
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // If the lacros service or the network settings service interface are not
    // available on this version of ash-chrome, this test suite will no-op.
    if (!IsServiceAvailable()) {
      return;
    }
    // Replace the production network settings service with a fake for testing.
    mojo::Remote<crosapi::mojom::NetworkSettingsService>& remote =
        chromeos::LacrosService::Get()
            ->GetRemote<crosapi::mojom::NetworkSettingsService>();
    remote.reset();
    receiver_.Bind(remote.BindNewPipeAndPassReceiver());

    alwayson_vpn_pre_connect_url_allowlist_observer_ =
        std::make_unique<NetworkSettingsObserver>(browser()->profile());

    base::RunLoop run_loop;
    service_.SetQuitClosure(run_loop.QuitClosure());
    alwayson_vpn_pre_connect_url_allowlist_observer_->Start();
    // Wait for the mojom::NetworkSettingsObserver to
    // be added as an observer to the AshNetworkSettingsService in Ash-Chrome.
    run_loop.Run();
  }

 protected:
  // Sends an updafe from the AshNetworkSettingService in Ash-Chrome and
  // waits for the update to be received by the Lacros-Chrome service.
  void SendAlwaysOnVpnPreConnectUrlAllowlistEnforcedAndWait(bool enforced) {
    service_.SetAlwaysOnVpnPreConnectUrlAllowlistEnforced(enforced);
    // This mojo notification from Ash will be eventually propagated to the
    // URLBlocklistManager in Lacros which schedules a task to update the
    // internal blocklist. It's not possible to observer when the internal
    // blocklist is updated so the test needs to wait for the message loop to
    // be empty.
    base::RunLoop().RunUntilIdle();
  }

  mojo::Remote<crosapi::mojom::NetworkSettingsService>
      network_settings_service_;
  std::unique_ptr<NetworkSettingsObserver>
      alwayson_vpn_pre_connect_url_allowlist_observer_;
  FakeNetworkSettingsService service_;
  mojo::Receiver<crosapi::mojom::NetworkSettingsService> receiver_{&service_};
};

// Tests that updates from AshNetworkService in Ash-Chrome are
// correctly propagated to  the PolicyBlocklistService in Lacros-Chrome.
IN_PROC_BROWSER_TEST_F(NetworkSettingsObserverTest, EnforceOverride) {
  if (!IsServiceAvailable()) {
    GTEST_SKIP() << "The Ash version is not supported.";
  }
  ASSERT_TRUE(service_.HasObservers());

  // URL configured by policy to bypass policy blocklist restrictions when the
  // pref `kAlwaysOnVpnPreConnectUrlAllowlist` is active.
  constexpr char kUrlAllowed[] = "http://google.com";
  // URL which is not included in the `kAlwaysOnVpnPreConnectUrlAllowlist` pref.
  constexpr char kUrlNeutral[] = "http://gmail.com";

  base::Value::List value;
  value.Append(kUrlAllowed);
  browser()->profile()->GetPrefs()->SetList(
      policy::policy_prefs::kAlwaysOnVpnPreConnectUrlAllowlist,
      std::move(value));

  PolicyBlocklistService* service =
      PolicyBlocklistFactory::GetForBrowserContext(browser()->profile());

  ASSERT_THAT(service, testing::NotNull());

  // Verify that connections to `kUrlNeutral` are allowed when
  // `kAlwaysOnVpnPreConnectUrlAllowlist` is not active.
  EXPECT_EQ(service->GetURLBlocklistState(GURL(kUrlNeutral)),
            policy::URLBlocklist::URLBlocklistState::URL_NEUTRAL_STATE);

  // Notify Lacros that it should enfroce the Always-on VPN pre-connect URL
  // allowlist.
  SendAlwaysOnVpnPreConnectUrlAllowlistEnforcedAndWait(/*enabled=*/true);

  // The URL specified in the `kAlwaysOnVpnPreConnectUrlAllowlist` is allowed to
  // proceed.
  EXPECT_EQ(service->GetURLBlocklistState(GURL(kUrlAllowed)),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);

  // URLs not covered by the URL filters listed in
  // `kAlwaysOnVpnPreConnectUrlAllowlist` are blocked.
  EXPECT_EQ(service->GetURLBlocklistState(GURL(kUrlNeutral)),
            policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST);

  // Verify that resetting the value works as intended.
  SendAlwaysOnVpnPreConnectUrlAllowlistEnforcedAndWait(/*enabled=*/false);

  EXPECT_EQ(service->GetURLBlocklistState(GURL(kUrlNeutral)),
            policy::URLBlocklist::URLBlocklistState::URL_NEUTRAL_STATE);
  EXPECT_EQ(service->GetURLBlocklistState(GURL(kUrlAllowed)),
            policy::URLBlocklist::URLBlocklistState::URL_NEUTRAL_STATE);
}
