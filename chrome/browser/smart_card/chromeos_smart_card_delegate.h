// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SMART_CARD_CHROMEOS_SMART_CARD_DELEGATE_H_
#define CHROME_BROWSER_SMART_CARD_CHROMEOS_SMART_CARD_DELEGATE_H_

#include "content/public/browser/smart_card_delegate.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

class ChromeOsSmartCardDelegate : public content::SmartCardDelegate {
 public:
  ChromeOsSmartCardDelegate();
  ~ChromeOsSmartCardDelegate() override;

  // `content::SmartCardDelegate` overrides:
  mojo::PendingRemote<device::mojom::SmartCardContextFactory>
  GetSmartCardContextFactory(content::BrowserContext& browser_context) override;
  bool IsPermissionBlocked(
      content::RenderFrameHost& render_frame_host) override;
  bool HasReaderPermission(content::RenderFrameHost& render_frame_host,
                           const std::string& reader_name) override;
  void RequestReaderPermission(
      content::RenderFrameHost& render_frame_host,
      const std::string& reader_name,
      RequestReaderPermissionCallback callback) override;

  void NotifyConnectionUsed(
      content::RenderFrameHost& render_frame_host) override;
  void NotifyLastConnectionLost(
      content::RenderFrameHost& render_frame_host) override;

  void AddObserver(content::RenderFrameHost& render_frame_host,
                   PermissionObserver* observer) override;
  void RemoveObserver(content::RenderFrameHost& render_frame_host,
                      PermissionObserver* observer) override;
};

#endif  // CHROME_BROWSER_SMART_CARD_CHROMEOS_SMART_CARD_DELEGATE_H_
