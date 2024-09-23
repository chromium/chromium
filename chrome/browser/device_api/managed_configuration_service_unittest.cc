// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_api/managed_configuration_service.h"

#include <memory>
#include <string_view>
#include <tuple>

#include "base/check_deref.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "url/gurl.h"

namespace {

constexpr std::string_view kUrl = "https://example.com";

std::tuple<ManagedConfigurationServiceImpl*,
           mojo::Remote<blink::mojom::ManagedConfigurationService>>
MaybeCreateService(content::WebContents* web_contents) {
  mojo::Remote<blink::mojom::ManagedConfigurationService> remote;
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents,
                                                             GURL(kUrl));
  return std::make_tuple(ManagedConfigurationServiceImpl::Create(
                             web_contents->GetPrimaryMainFrame(),
                             remote.BindNewPipeAndPassReceiver()),
                         std::move(remote));
}

std::tuple<ManagedConfigurationServiceImpl&,
           mojo::Remote<blink::mojom::ManagedConfigurationService>>
CreateService(content::WebContents* web_contents) {
  auto [service, remote] = MaybeCreateService(web_contents);
  return std::forward_as_tuple(CHECK_DEREF(service), std::move(remote));
}

// Observer that surfaces when `OnConfigurationChanged` is called.
class ChangeObserver : public blink::mojom::ManagedConfigurationObserver {
 public:
  ChangeObserver() = default;
  ChangeObserver(const ChangeObserver&) = delete;
  ChangeObserver& operator=(const ChangeObserver&) = delete;
  ~ChangeObserver() override = default;

  // blink::mojom::ManagedConfigurationObserver overrides.
  void OnConfigurationChanged() override { did_change_ = true; }

  bool did_change() {
    // Process pending messages that may be in flight.
    receiver_.FlushForTesting();
    return did_change_;
  }

  mojo::PendingRemote<blink::mojom::ManagedConfigurationObserver> bind() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  bool did_change_ = false;
  mojo::Receiver<blink::mojom::ManagedConfigurationObserver> receiver_{this};
};

}  // namespace

using ManagedConfigurationServiceTest = ChromeRenderViewHostTestHarness;

TEST_F(ManagedConfigurationServiceTest,
       ChangeNotificationWithoutObserversDoesNotCrash) {
  auto [service, remote] = CreateService(web_contents());
  service.OnManagedConfigurationChanged();
}

TEST_F(ManagedConfigurationServiceTest, NotifiesChangesToObserver) {
  auto [service, remote] = CreateService(web_contents());

  ChangeObserver observer;
  service.SubscribeToManagedConfiguration(observer.bind());
  EXPECT_FALSE(observer.did_change());

  service.OnManagedConfigurationChanged();
  EXPECT_TRUE(observer.did_change());
}

TEST_F(ManagedConfigurationServiceTest, SupportsOneObserverAtATime) {
  auto [service, remote] = CreateService(web_contents());

  {
    ChangeObserver observer1;
    service.SubscribeToManagedConfiguration(observer1.bind());
  }

  // `observer1` was destroyed above, flush so `service` gets the disconnect.
  remote.FlushForTesting();

  ChangeObserver observer2;
  service.SubscribeToManagedConfiguration(observer2.bind());
  EXPECT_FALSE(observer2.did_change());
  service.OnManagedConfigurationChanged();
  EXPECT_TRUE(observer2.did_change());
}

TEST_F(ManagedConfigurationServiceTest, IsBoundInNormalProfile) {
  auto [service, remote] = MaybeCreateService(web_contents());
  ASSERT_NE(service, nullptr);

  remote.FlushForTesting();
  ASSERT_TRUE(remote.is_connected());
}

TEST_F(ManagedConfigurationServiceTest, IsNotBoundInIncognito) {
  std::unique_ptr<content::WebContents> incognito_web_contents =
      content::WebContentsTester::CreateTestWebContents(
          profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true), nullptr);

  auto [service, remote] = MaybeCreateService(incognito_web_contents.get());
  ASSERT_EQ(service, nullptr);

  remote.FlushForTesting();
  ASSERT_FALSE(remote.is_connected());
}

class ManagedConfigurationServiceGuestTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ASSERT_TRUE(profile_manager_.SetUp());
  }

  void TearDown() override {
    profile_manager_.DeleteAllTestingProfiles();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfileManager& profile_manager() { return profile_manager_; }

 private:
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
};

TEST_F(ManagedConfigurationServiceGuestTest, IsNotBoundInGuestProfile) {
  std::unique_ptr<content::WebContents> guest_web_contents =
      content::WebContentsTester::CreateTestWebContents(
          profile_manager().CreateGuestProfile()->GetPrimaryOTRProfile(
              /*create_if_needed=*/true),
          /*instance=*/nullptr);

  auto [service, remote] = MaybeCreateService(guest_web_contents.get());
  ASSERT_EQ(service, nullptr);

  remote.FlushForTesting();
  ASSERT_FALSE(remote.is_connected());
}
