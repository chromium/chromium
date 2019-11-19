// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_HID_CHOOSER_CONTEXT_H_
#define CHROME_BROWSER_HID_HID_CHOOSER_CONTEXT_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/permissions/chooser_context_base.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "url/origin.h"

namespace base {
class Value;
}

// Manages the internal state and connection to the device service for the
// Human Interface Device (HID) chooser UI.
class HidChooserContext : public ChooserContextBase {
 public:
  explicit HidChooserContext(Profile* profile);
  ~HidChooserContext() override;

  // Given a chooser item |object|, returns a human-readable string
  // representing the object.
  static std::string GetObjectName(const base::Value& object);

  // ChooserContextBase:
  bool IsValidObject(const base::Value& object) override;
  // In addition these methods from ChooserContextBase are overridden in order
  // to expose ephemeral devices through the public interface.
  std::vector<std::unique_ptr<Object>> GetGrantedObjects(
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin) override;
  std::vector<std::unique_ptr<Object>> GetAllGrantedObjects() override;
  void RevokeObjectPermission(const url::Origin& requesting_origin,
                              const url::Origin& embedding_origin,
                              const base::Value& object) override;

  // HID-specific interface for granting and checking permissions.
  void GrantDevicePermission(const url::Origin& requesting_origin,
                             const url::Origin& embedding_origin,
                             const device::mojom::HidDeviceInfo& device);
  bool HasDevicePermission(const url::Origin& requesting_origin,
                           const url::Origin& embedding_origin,
                           const device::mojom::HidDeviceInfo& device);

  device::mojom::HidManager* GetHidManager();

  void SetHidManagerForTesting(
      mojo::PendingRemote<device::mojom::HidManager> manager);
  base::WeakPtr<HidChooserContext> AsWeakPtr();

 private:
  void EnsureHidManagerConnection();
  void SetUpHidManagerConnection(
      mojo::PendingRemote<device::mojom::HidManager> manager);
  void OnHidManagerConnectionError();

  const bool is_incognito_;

  // Tracks the set of devices to which an origin (potentially embedded in
  // another origin) has access to. Key is (requesting_origin,
  // embedding_origin).
  std::map<std::pair<url::Origin, url::Origin>, std::set<std::string>>
      ephemeral_devices_;

  // Holds information about devices in |ephemeral_devices_|.
  std::map<std::string, base::Value> device_info_;

  mojo::Remote<device::mojom::HidManager> hid_manager_;

  base::WeakPtrFactory<HidChooserContext> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HidChooserContext);
};

#endif  // CHROME_BROWSER_HID_HID_CHOOSER_CONTEXT_H_
