// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_AUDIO_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_AUDIO_SERVICE_ASH_H_

#include "base/scoped_observation.h"
#include "chromeos/crosapi/mojom/audio_service.mojom.h"
#include "extensions/browser/api/audio/audio_service.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace extensions {
class AudioDeviceIdCalculator;
}  // namespace extensions

class Profile;

namespace crosapi {

// Implements the crosapi interface for audio service API.
class AudioServiceAsh : public mojom::AudioService {
 public:
  AudioServiceAsh();
  AudioServiceAsh(const AudioServiceAsh&) = delete;
  AudioServiceAsh& operator=(const AudioServiceAsh&) = delete;
  ~AudioServiceAsh() override;

  void Initialize(Profile* profile);
  void BindReceiver(mojo::PendingReceiver<mojom::AudioService> receiver);

  // crosapi::mojom::AudioDevices:
  void GetDevices(mojom::DeviceFilterPtr filter,
                  GetDevicesCallback callback) override;
  void GetMute(mojom::StreamType stream_type,
               GetMuteCallback callback) override;
  void SetActiveDeviceLists(mojom::DeviceIdListsPtr ids,
                            SetActiveDeviceListsCallback callback) override;
  void SetMute(mojom::StreamType stream_type,
               bool is_muted,
               SetMuteCallback callback) override;
  void SetProperties(const std::string& id,
                     mojom::AudioDevicePropertiesPtr properties,
                     SetPropertiesCallback callback) override;

  void AddAudioChangeObserver(
      mojo::PendingRemote<mojom::AudioChangeObserver> observer) override;

 private:
  class Observer : public extensions::AudioService::Observer {
   public:
    Observer();
    ~Observer() override;

    void Initialize(extensions::AudioService* service);

    // extensions::AudioService::Observer implementation:
    void OnLevelChanged(const std::string& id, int level) override;
    void OnMuteChanged(bool is_input, bool is_muted) override;
    void OnDevicesChanged(const extensions::DeviceInfoList& devices) override;

    void AddCrosapiObserver(
        mojo::PendingRemote<mojom::AudioChangeObserver> observer);

   private:
    base::ScopedObservation<extensions::AudioService,
                            extensions::AudioService::Observer>
        audio_service_observation_{this};

    // Support any number of observers.
    mojo::RemoteSet<mojom::AudioChangeObserver> observers_;
  };

  // Any number of crosapi clients can connect to this class.
  mojo::ReceiverSet<mojom::AudioService> receivers_;

  std::unique_ptr<extensions::AudioDeviceIdCalculator> stable_id_calculator_;
  std::unique_ptr<extensions::AudioService> service_;

  // Observer must be defined after service for a correct destruction order.
  Observer observer_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_AUDIO_SERVICE_ASH_H_
