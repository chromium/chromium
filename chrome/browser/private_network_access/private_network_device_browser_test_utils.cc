// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_network_access/private_network_device_browser_test_utils.h"

#include "chrome/browser/private_network_access/chrome_private_network_device_chooser.h"
#include "chrome/browser/private_network_access/private_network_device_chooser_controller.h"
#include "components/permissions/chooser_controller.h"
#include "content/public/common/content_client.h"

class FakeChooserView : public permissions::ChooserController::View {
 public:
  explicit FakeChooserView(
      std::unique_ptr<permissions::ChooserController> controller)
      : controller_(std::move(controller)) {
    controller_->set_view(this);
  }

  FakeChooserView(const FakeChooserView&) = delete;
  FakeChooserView& operator=(const FakeChooserView&) = delete;

  ~FakeChooserView() override { controller_->set_view(nullptr); }

  void OnOptionsInitialized() override {
    if (controller_->NumOptions()) {
      controller_->Select({0});
    } else {
      controller_->Cancel();
    }
    delete this;
  }

  void OnOptionAdded(size_t index) override { NOTREACHED_IN_MIGRATION(); }
  void OnOptionRemoved(size_t index) override { NOTREACHED_IN_MIGRATION(); }
  void OnOptionUpdated(size_t index) override { NOTREACHED_IN_MIGRATION(); }
  void OnAdapterEnabledChanged(bool enabled) override {
    NOTREACHED_IN_MIGRATION();
  }
  void OnRefreshStateChanged(bool refreshing) override {
    NOTREACHED_IN_MIGRATION();
  }

 private:
  std::unique_ptr<permissions::ChooserController> controller_;
};

class FakePNAChooser : public ChromePrivateNetworkDeviceChooser {
 public:
  FakePNAChooser() = default;
  FakePNAChooser(const FakePNAChooser&) = delete;
  FakePNAChooser& operator=(const FakePNAChooser&) = delete;
  ~FakePNAChooser() override = default;

  void ShowChooser(content::RenderFrameHost* frame,
                   std::unique_ptr<PrivateNetworkDeviceChooserController>
                       controller) override {
    // Device list initialization in PrivateNetworkDeviceChooserController may
    // complete before having a valid view in which case OnOptionsInitialized()
    // has no chance to be triggered, so select the first option directly if
    // options are ready.
    if (controller->NumOptions()) {
      controller->Select({0});
    } else {
      new FakeChooserView(std::move(controller));
    }
  }
};

TestPNADelegate::TestPNADelegate() = default;
TestPNADelegate::~TestPNADelegate() = default;

void TestPNADelegate::RequestPermission(
    content::RenderFrameHost& frame,
    blink::mojom::PrivateNetworkDevicePtr device,
    network::mojom::URLLoaderNetworkServiceObserver::
        OnPrivateNetworkAccessPermissionRequiredCallback callback) {
  bool is_device_valid = CheckDevice(*device, frame);
  if (HasDevicePermission(frame, *device, is_device_valid)) {
    std::move(callback).Run(true);
    return;
  }
  RunChooser(frame, std::move(device), std::move(callback), is_device_valid);
}

std::unique_ptr<ChromePrivateNetworkDeviceChooser> TestPNADelegate::RunChooser(
    content::RenderFrameHost& frame,
    blink::mojom::PrivateNetworkDevicePtr device,
    network::mojom::URLLoaderNetworkServiceObserver::
        OnPrivateNetworkAccessPermissionRequiredCallback callback,
    bool is_device_valid) {
  auto chooser = std::make_unique<FakePNAChooser>();
  chooser->ShowChooser(
      &frame, std::make_unique<PrivateNetworkDeviceChooserController>(
                  &frame, std::move(device),
                  base::BindOnce(&ChromePrivateNetworkDeviceDelegate::
                                     HandlePrivateNetworkDeviceChooserResult,
                                 base::Unretained(this), is_device_valid,
                                 std::move(callback))));
  return chooser;
}

TestPNAContentBrowserClient::TestPNAContentBrowserClient()
    : pna_delegate_(std::make_unique<TestPNADelegate>()) {}
TestPNAContentBrowserClient::~TestPNAContentBrowserClient() = default;

ChromePrivateNetworkDeviceDelegate*
TestPNAContentBrowserClient::GetPrivateNetworkDeviceDelegate() {
  return pna_delegate_.get();
}

void TestPNAContentBrowserClient::SetAsBrowserClient() {
  original_content_browser_client_ = content::SetBrowserClientForTesting(this);
}

void TestPNAContentBrowserClient::UnsetAsBrowserClient() {
  content::SetBrowserClientForTesting(original_content_browser_client_);
  pna_delegate_.reset();
}
