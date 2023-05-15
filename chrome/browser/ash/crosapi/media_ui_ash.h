// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_MEDIA_UI_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_MEDIA_UI_ASH_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/media_ui.mojom.h"
#include "components/global_media_controls/public/mojom/device_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace crosapi {

class MediaUIAsh : public mojom::MediaUI {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // The pointer to `device_service` may be invalidated after this call exits,
    // so it should not be stored.
    virtual void OnDeviceServiceRegistered(
        global_media_controls::mojom::DeviceService* device_service) = 0;
  };

  MediaUIAsh();
  MediaUIAsh(const MediaUIAsh&) = delete;
  MediaUIAsh& operator=(const MediaUIAsh&) = delete;
  ~MediaUIAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::MediaUI> receiver);

  // mojom::MediaUI:
  void RegisterDeviceService(
      const base::UnguessableToken& id,
      mojo::PendingRemote<global_media_controls::mojom::DeviceService>
          pending_device_service) override;
  void ShowDevicePicker(const std::string& item_id) override;

  // Returns the DeviceService associated with `id`, if it exists.
  global_media_controls::mojom::DeviceService* GetDeviceService(
      const base::UnguessableToken& id);
  const std::map<base::UnguessableToken,
                 mojo::Remote<global_media_controls::mojom::DeviceService>>&
  device_services() const {
    return device_services_;
  }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  void RemoveDeviceService(const base::UnguessableToken& id);

  mojo::ReceiverSet<mojom::MediaUI> receivers_;
  std::map<base::UnguessableToken,
           mojo::Remote<global_media_controls::mojom::DeviceService>>
      device_services_;
  base::ObserverList<Observer> observers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_MEDIA_UI_ASH_H_
