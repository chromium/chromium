// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVATE_NETWORK_ACCESS_PRIVATE_NETWORK_DEVICE_BROWSER_TEST_UTILS_H_
#define CHROME_BROWSER_PRIVATE_NETWORK_ACCESS_PRIVATE_NETWORK_DEVICE_BROWSER_TEST_UTILS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/private_network_access/chrome_private_network_device_delegate.h"

class TestPNADelegate : public ChromePrivateNetworkDeviceDelegate {
 public:
  TestPNADelegate();
  TestPNADelegate(const TestPNADelegate&) = delete;
  TestPNADelegate& operator=(const TestPNADelegate&) = delete;
  ~TestPNADelegate() override;

  void RequestPermission(
      content::RenderFrameHost& frame,
      blink::mojom::PrivateNetworkDevicePtr device,
      network::mojom::URLLoaderNetworkServiceObserver::
          OnPrivateNetworkAccessPermissionRequiredCallback callback) override;

  std::unique_ptr<ChromePrivateNetworkDeviceChooser> RunChooser(
      content::RenderFrameHost& frame,
      blink::mojom::PrivateNetworkDevicePtr device,
      network::mojom::URLLoaderNetworkServiceObserver::
          OnPrivateNetworkAccessPermissionRequiredCallback callback,
      bool is_device_valid);
};

class TestPNAContentBrowserClient : public ChromeContentBrowserClient {
 public:
  TestPNAContentBrowserClient();
  TestPNAContentBrowserClient(const TestPNAContentBrowserClient&) = delete;
  TestPNAContentBrowserClient& operator=(const TestPNAContentBrowserClient&) =
      delete;
  ~TestPNAContentBrowserClient() override;

  // ChromeContentBrowserClient:
  ChromePrivateNetworkDeviceDelegate* GetPrivateNetworkDeviceDelegate()
      override;

  TestPNADelegate& delegate() { return *pna_delegate_; }

  void SetAsBrowserClient();
  void UnsetAsBrowserClient();

 private:
  std::unique_ptr<TestPNADelegate> pna_delegate_;
  raw_ptr<content::ContentBrowserClient> original_content_browser_client_;
};

#endif  // CHROME_BROWSER_PRIVATE_NETWORK_ACCESS_PRIVATE_NETWORK_DEVICE_BROWSER_TEST_UTILS_H_
