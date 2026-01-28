// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SMART_CARD_CHROMEOS_SMART_CARD_DELEGATE_H_
#define CHROME_BROWSER_SMART_CARD_CHROMEOS_SMART_CARD_DELEGATE_H_

#include "base/containers/flat_map.h"
#include "content/public/browser/smart_card_delegate.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

// TODO(crbug.com/400996808): Split this class into ChromeOS-specific and
// system-agnostic code.
class ChromeOsSmartCardDelegate : public content::SmartCardDelegate {
 public:
  ChromeOsSmartCardDelegate();
  ~ChromeOsSmartCardDelegate() override;

  // `content::SmartCardDelegate` overrides:
  mojo::PendingRemote<device::mojom::SmartCardContextFactory>
  GetSmartCardContextFactory(
      content::RenderFrameHost& render_frame_host) override;

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

  void SetEmulationFactory(
      content::GlobalRenderFrameHostId frame_id,
      base::RepeatingCallback<
          mojo::PendingRemote<device::mojom::SmartCardContextFactory>()>
          factory_getter) override;

  void ClearEmulationFactory(
      content::GlobalRenderFrameHostId frame_id) override;

 private:
  mojo::PendingRemote<device::mojom::SmartCardContextFactory>
  GetEmulationFactory(content::GlobalRenderFrameHostId frame_id);

  // Stores the overrides per frame.
  base::flat_map<content::GlobalRenderFrameHostId,
                 base::RepeatingCallback<mojo::PendingRemote<
                     device::mojom::SmartCardContextFactory>()>>
      emulation_overrides_;
};

#endif  // CHROME_BROWSER_SMART_CARD_CHROMEOS_SMART_CARD_DELEGATE_H_
