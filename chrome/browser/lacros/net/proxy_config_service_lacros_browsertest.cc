// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/net/proxy_config_service_lacros.h"

#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lacros/net/network_settings_translation.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/network_settings_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/proxy_resolution/proxy_config_service.h"

namespace {

constexpr char kPacUrl[] = "pac.pac";

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

  bool HasObservers() { return !observers_.empty(); }

  // crosapi::mojom::AshNetworService:
  void AddNetworkSettingsObserver(
      mojo::PendingRemote<crosapi::mojom::NetworkSettingsObserver> observer)
      override {
    mojo::Remote<crosapi::mojom::NetworkSettingsObserver> remote(
        std::move(observer));
    if (cached_proxy_config_) {
      remote->OnProxyChanged(cached_proxy_config_.Clone());
    }
    observers_.Add(std::move(remote));

    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  void IsAlwaysOnVpnPreConnectUrlAllowlistEnforced(
      IsAlwaysOnVpnPreConnectUrlAllowlistEnforcedCallback callback) override {}

  // This fake implementation of the crosapi::mojom::NetworkSettingsService is
  // only used to test the behaviour of `ProxyConfigServiceLacros`, which is an
  // observer of the mojo service. Observers only listen for updates, they do
  // not send data to the service. Extension set proxy are tested by the test
  // suite LacrosExtensionProxyTrackerTest whose fixture supports installing
  // extension.
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

  void SendProxyUpdate(crosapi::mojom::ProxyConfigPtr proxy_config) {
    cached_proxy_config_ = std::move(proxy_config);
    for (const auto& obs : observers_) {
      obs->OnProxyChanged(cached_proxy_config_.Clone());
    }
  }

 private:
  crosapi::mojom::ProxyConfigPtr cached_proxy_config_;
  mojo::RemoteSet<crosapi::mojom::NetworkSettingsObserver> observers_;
  base::OnceClosure quit_closure_;
};

// Fakes the ProxyConfigMonitor which is the class that sends the proxy updates
// to the Browser's NetworkService process via mojo.
class FakeProxyMonitor : public net::ProxyConfigService::Observer {
 public:
  FakeProxyMonitor() = default;
  FakeProxyMonitor(const FakeProxyMonitor&) = delete;
  FakeProxyMonitor& operator=(const FakeProxyMonitor&) = delete;
  ~FakeProxyMonitor() override = default;

  void OnProxyConfigChanged(
      const net::ProxyConfigWithAnnotation& config,
      net::ProxyConfigService::ConfigAvailability availability) override {
    cached_proxy_config_ = std::move(config);
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  void SetQuitClosure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }
  net::ProxyConfigWithAnnotation cached_proxy_config_;

 private:
  base::OnceClosure quit_closure_;
};

}  // namespace

namespace chromeos {

class ProxyConfigServiceLacrosTest : public InProcessBrowserTest {
 protected:
  ProxyConfigServiceLacrosTest() = default;

  ProxyConfigServiceLacrosTest(const ProxyConfigServiceLacrosTest&) = delete;
  ProxyConfigServiceLacrosTest& operator=(const ProxyConfigServiceLacrosTest&) =
      delete;
  ~ProxyConfigServiceLacrosTest() override = default;
  mojo::Remote<crosapi::mojom::NetworkSettingsService>
      network_settings_service_;

  bool IsServiceAvailable() {
    auto* lacros_service = chromeos::LacrosService::Get();
    if (!lacros_service)
      return false;
    return lacros_service
        ->IsAvailable<crosapi::mojom::NetworkSettingsService>();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // If the lacros service or the network settings service interface are not
    // available on this version of ash-chrome, this test suite will no-op.
    if (!IsServiceAvailable())
      return;
    // Replace the production network settings service with a fake for testing.
    mojo::Remote<crosapi::mojom::NetworkSettingsService>& remote =
        chromeos::LacrosService::Get()
            ->GetRemote<crosapi::mojom::NetworkSettingsService>();
    remote.reset();
    receiver_.Bind(remote.BindNewPipeAndPassReceiver());
    SetupProxyMonitoring(browser()->profile());
  }

  void TearDownOnMainThread() override { ResetProxyMonitoring(); }

 protected:
  // Sends a proxy update via the AshNetworkSettingService in Ash-Chrome and
  // waits for `proxy_monitor_` in Lacros-Chrome to receive the config.
  void SendAshProxyUpdateAndWait(crosapi::mojom::ProxyConfigPtr proxy_config) {
    base::RunLoop run_loop;
    proxy_monitor_->SetQuitClosure(run_loop.QuitClosure());
    service_.SendProxyUpdate(std::move(proxy_config));
    run_loop.Run();
  }

  // Creates the objects that monitor proxy configs coming from Ash and from
  // profile prefs.
  void SetupProxyMonitoring(Profile* profile) {
    pref_proxy_config_tracker_ =
        ProxyServiceFactory::CreatePrefProxyConfigTrackerOfProfile(
            profile->GetPrefs(), nullptr);

    base::RunLoop run_loop;
    service_.SetQuitClosure(run_loop.QuitClosure());
    proxy_config_service_ = ProxyServiceFactory::CreateProxyConfigService(
        pref_proxy_config_tracker_.get(), profile);
    // Wait for the mojom::NetworkSettingsObserver created by
    // `proxy_config_service_` in Lacros to be added as an observer to the
    // AshNetworkSettingsService in Ash-Chrome.
    run_loop.Run();

    proxy_monitor_ = std::make_unique<FakeProxyMonitor>();
    proxy_config_service_->AddObserver(proxy_monitor_.get());
  }

  void ResetProxyMonitoring() {
    proxy_config_service_->RemoveObserver(proxy_monitor_.get());
    pref_proxy_config_tracker_->DetachFromPrefService();
    pref_proxy_config_tracker_.reset();
    proxy_config_service_.reset();
    proxy_monitor_.reset();
    base::RunLoop().RunUntilIdle();
  }

  Profile& CreateSecondaryProfile() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath profile_path =
        profile_manager->GenerateNextProfileDirectoryPath();
    return profiles::testing::CreateProfileSync(profile_manager, profile_path);
  }

  std::unique_ptr<net::ProxyConfigService> proxy_config_service_;
  // Monitors global and profile prefs related to proxy configuration.
  std::unique_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;
  // Fakes the ProxyConfigMonitor which directly updates the Browser's
  // NetworkService process.
  std::unique_ptr<FakeProxyMonitor> proxy_monitor_;
  FakeNetworkSettingsService service_;
  mojo::Receiver<crosapi::mojom::NetworkSettingsService> receiver_{&service_};
};

// Tests that the chromeos::ProxyConfigServiceLacros instance internally created
// by `proxy_config_service_` is added to Ash-Chrome as an observer for network
// changes.
IN_PROC_BROWSER_TEST_F(ProxyConfigServiceLacrosTest, ObserverSet) {
  if (!IsServiceAvailable())
    return;
  ASSERT_TRUE(service_.HasObservers());
}

// Tests that proxy updates from the AshNetworkService in Ash-Chrome are
// correctly propagated to observers of the `proxy_config_service_` in
// Lacros-Chrome.
IN_PROC_BROWSER_TEST_F(ProxyConfigServiceLacrosTest, ProxyUpdates) {
  if (!IsServiceAvailable())
    return;

  crosapi::mojom::ProxyConfigPtr proxy_config =
      crosapi::mojom::ProxyConfig::New();

  proxy_config->proxy_settings = crosapi::mojom::ProxySettings::NewDirect(
      crosapi::mojom::ProxySettingsDirect::New());
  SendAshProxyUpdateAndWait(proxy_config.Clone());
  EXPECT_EQ(proxy_monitor_->cached_proxy_config_.value().ToValue(),
            net::ProxyConfig::CreateDirect().ToValue());

  crosapi::mojom::ProxySettingsWpadPtr wpad =
      crosapi::mojom::ProxySettingsWpad::New();
  wpad->pac_url = GURL(kPacUrl);
  // TODO(crbug.com/40223591): This test seems buggy; wpad is never used.
  proxy_config->proxy_settings = crosapi::mojom::ProxySettings::NewWpad(
      crosapi::mojom::ProxySettingsWpad::New());
  SendAshProxyUpdateAndWait(proxy_config.Clone());
  EXPECT_EQ(proxy_monitor_->cached_proxy_config_.value().ToValue(),
            CrosapiProxyToNetProxy(proxy_config.Clone()).value().ToValue());

  crosapi::mojom::ProxySettingsManualPtr manual =
      crosapi::mojom::ProxySettingsManual::New();
  crosapi::mojom::ProxyLocationPtr location =
      crosapi::mojom::ProxyLocation::New();
  location->host = "proxy";
  location->port = 80;
  manual->secure_http_proxies.push_back(location.Clone());
  manual->socks_proxies.push_back(location.Clone());
  proxy_config->proxy_settings->set_manual(std::move(manual));
  SendAshProxyUpdateAndWait(proxy_config.Clone());
  EXPECT_EQ(proxy_monitor_->cached_proxy_config_.value().ToValue(),
            CrosapiProxyToNetProxy(std::move(proxy_config)).value().ToValue());
}

// Tests that proxies set via the user pref `kProxy` have precedence over system
// proxies coming from Ash-Chrome.
IN_PROC_BROWSER_TEST_F(ProxyConfigServiceLacrosTest, UserPrefPrecedence) {
  if (!IsServiceAvailable())
    return;
  // Set a proxy via pref.
  base::RunLoop run_loop;
  proxy_monitor_->SetQuitClosure(run_loop.QuitClosure());
  base::Value::Dict proxy_config_wpad;
  proxy_config_wpad.Set("mode", ProxyPrefs::kAutoDetectProxyModeName);
  browser()->profile()->GetPrefs()->SetDict(proxy_config::prefs::kProxy,
                                            std::move(proxy_config_wpad));
  run_loop.Run();
  // Verify that the pref proxy is applied.
  EXPECT_EQ(proxy_monitor_->cached_proxy_config_.value().ToValue(),
            net::ProxyConfig::CreateAutoDetect().ToValue());

  // Set a system proxy via the AshNetworkSettingsService.
  crosapi::mojom::ProxyConfigPtr proxy_config =
      crosapi::mojom::ProxyConfig::New();
  crosapi::mojom::ProxySettingsPacPtr pac =
      crosapi::mojom::ProxySettingsPac::New();
  pac->pac_url = GURL(kPacUrl);
  proxy_config->proxy_settings =
      crosapi::mojom::ProxySettings::NewPac(std::move(pac));
  service_.SendProxyUpdate(std::move(proxy_config));
  base::RunLoop().RunUntilIdle();

  // Verify that the pref proxy is still applied.
  EXPECT_EQ(proxy_monitor_->cached_proxy_config_.value().ToValue(),
            net::ProxyConfig::CreateAutoDetect().ToValue());

  // Remove the pref and verify that the system proxy is applied.
  browser()->profile()->GetPrefs()->ClearPref(proxy_config::prefs::kProxy);
  EXPECT_EQ(proxy_monitor_->cached_proxy_config_.value().ToValue(),
            CrosapiProxyToNetProxy(std::move(proxy_config)).value().ToValue());
}

// Test that verifies that for secondary profiles, the system proxy sent from
// Ash-Chrome is only applied if the pref `kUseAshProxy` is true.
IN_PROC_BROWSER_TEST_F(ProxyConfigServiceLacrosTest, UseAshProxyPref) {
  if (!IsServiceAvailable())
    return;
  const base::Value expectedDirect = net::ProxyConfig::CreateDirect().ToValue();
  const base::Value expectedAutoDetect =
      net::ProxyConfig::CreateAutoDetect().ToValue();

  ResetProxyMonitoring();
  Profile& profile = CreateSecondaryProfile();
  SetupProxyMonitoring(&profile);

  profile.GetPrefs()->SetBoolean(prefs::kUseAshProxy, false);
  crosapi::mojom::ProxyConfigPtr proxy_config =
      crosapi::mojom::ProxyConfig::New();
  proxy_config->proxy_settings = crosapi::mojom::ProxySettings::NewWpad(
      crosapi::mojom::ProxySettingsWpad::New());
  service_.SendProxyUpdate(proxy_config.Clone());
  base::RunLoop().RunUntilIdle();
  // Verify that the system proxy is not applied.
  EXPECT_EQ(proxy_monitor_->cached_proxy_config_.value().ToValue(),
            expectedDirect);

  profile.GetPrefs()->SetBoolean(prefs::kUseAshProxy, true);
  // Verify that the system proxy is applied.
  EXPECT_EQ(proxy_monitor_->cached_proxy_config_.value().ToValue(),
            expectedAutoDetect);

  // Verify that the system proxy is not applied.
  profile.GetPrefs()->SetBoolean(prefs::kUseAshProxy, false);
  EXPECT_EQ(proxy_monitor_->cached_proxy_config_.value().ToValue(),
            expectedDirect);
}

// Verifies that the proxy is only applied incognito if it's not controlled by a
// an extension active in the main Lacros profile.
IN_PROC_BROWSER_TEST_F(ProxyConfigServiceLacrosTest, IncognitoProfile) {
  if (!IsServiceAvailable())
    return;
  const base::Value expectedDirect = net::ProxyConfig::CreateDirect().ToValue();
  const base::Value expectedAutoDetect =
      net::ProxyConfig::CreateAutoDetect().ToValue();

  ResetProxyMonitoring();
  auto* incognito_profile =
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  SetupProxyMonitoring(incognito_profile);

  // kUseAshProxy should be ignored for the incognito profile.
  incognito_profile->GetPrefs()->SetBoolean(prefs::kUseAshProxy, true);

  crosapi::mojom::ProxyConfigPtr proxy_config =
      crosapi::mojom::ProxyConfig::New();
  proxy_config->proxy_settings = crosapi::mojom::ProxySettings::NewWpad(
      crosapi::mojom::ProxySettingsWpad::New());
  proxy_config->extension = crosapi::mojom::ExtensionControllingProxy::New();
  service_.SendProxyUpdate(proxy_config.Clone());
  base::RunLoop().RunUntilIdle();

  // Verify that the system proxy controlled by the pref is not applied.
  EXPECT_EQ(proxy_monitor_->cached_proxy_config_.value().ToValue(),
            expectedDirect);

  proxy_config->extension.reset();
  SendAshProxyUpdateAndWait(std::move(proxy_config));

  // Verify that the system proxy is applied.
  EXPECT_EQ(proxy_monitor_->cached_proxy_config_.value().ToValue(),
            expectedAutoDetect);
}

}  // namespace chromeos
