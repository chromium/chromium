// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network/network_portal_signin_window.h"

#include "chrome/browser/chromeos/network/network_portal_signin_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/network_change.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

class FakeNetworkChange : public crosapi::mojom::NetworkChange {
 public:
  FakeNetworkChange() = default;
  FakeNetworkChange(const FakeNetworkChange&) = delete;
  FakeNetworkChange& operator=(const FakeNetworkChange&) = delete;
  ~FakeNetworkChange() override = default;

  // crosapi::mojom::NetworkChange:
  void AddObserver(mojo::PendingRemote<crosapi::mojom::NetworkChangeObserver>
                       observer) override {}
  void RequestPortalDetection() override { portal_detection_requested_++; }

  int portal_detection_requested() { return portal_detection_requested_; }

 private:
  int portal_detection_requested_ = 0;
};

}  // namespace

class NetworkPortalSigninWindowLacrosBrowserTest : public InProcessBrowserTest {
 public:
  NetworkPortalSigninWindowLacrosBrowserTest() = default;
  ~NetworkPortalSigninWindowLacrosBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(NetworkPortalSigninWindowLacrosBrowserTest,
                       RequestPortalDetection) {
  auto* lacros_service = LacrosService::Get();
  ASSERT_TRUE(lacros_service);
  ASSERT_TRUE(lacros_service->IsAvailable<crosapi::mojom::NetworkChange>());

  FakeNetworkChange fake_network_change;
  mojo::Receiver<FakeNetworkChange> fake_network_change_receiver(
      &fake_network_change);
  lacros_service->InjectRemoteForTesting(
      fake_network_change_receiver.BindNewPipeAndPassRemote());

  content::CreateAndLoadWebContentsObserver web_contents_observer;

  NetworkPortalSigninWindow::Get()->Show(
      GURL("http://www.gstatic.com/generate_204"));
  ASSERT_TRUE(NetworkPortalSigninWindow::Get()->GetBrowserForTesting());

  web_contents_observer.Wait();

  // Showing the window should generate a DidFinishNavigation event which should
  // trigger a corresponding RequestPortalDetection call.
  EXPECT_GE(fake_network_change.portal_detection_requested(), 1);
}

}  // namespace chromeos
