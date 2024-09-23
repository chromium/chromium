// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MULTI_CAPTURE_MULTI_CAPTURE_SERVICE_CLIENT_H_
#define ASH_MULTI_CAPTURE_MULTI_CAPTURE_SERVICE_CLIENT_H_

#include <string>

#include "ash/ash_export.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/multi_capture_service.mojom.h"
#include "url/origin.h"

namespace ash {

// Client of the MultiCaptureService mojo interface. Receives events about
// multi captures being started / stopped and forwards it to ash clients to
// show usage indicators.
class ASH_EXPORT MultiCaptureServiceClient
    : public video_capture::mojom::MultiCaptureServiceClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Event to inform about a started multi capture. The `label` is a unique
    // identifier that can be used to connect started / stopped events.
    // The `origin` is the capturer's origin.
    // TODO(crbug.com/40225619): Consider transferred tracks by either adding
    // a MultiCaptureTransferred event or by making sure the label remains
    // constant throughout the lifetime of the capture.
    virtual void MultiCaptureStarted(const std::string& label,
                                     const url::Origin& origin) = 0;
    // Event to inform that an app started multi capture. The `label` is a
    // unique identifier that can be used to connect started / stopped events.
    // The `app_id` is the id of the calling app (as given in the app service)
    // and `app_short_name` is the short name of the app (derived from the app
    // manifest or the app title).
    virtual void MultiCaptureStartedFromApp(
        const std::string& label,
        const std::string& app_id,
        const std::string& app_short_name) = 0;
    virtual void MultiCaptureStopped(const std::string& label) = 0;
    virtual void MultiCaptureServiceClientDestroyed() = 0;

   protected:
    ~Observer() override = default;
  };

  explicit MultiCaptureServiceClient(
      mojo::PendingRemote<video_capture::mojom::MultiCaptureService>
          multi_capture_service);
  ~MultiCaptureServiceClient() override;
  MultiCaptureServiceClient(const MultiCaptureServiceClient&) = delete;
  MultiCaptureServiceClient& operator=(const MultiCaptureServiceClient&) =
      delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // video_capture::mojom::MultiCaptureService:
  void MultiCaptureStarted(const std::string& label,
                           const url::Origin& origin) override;
  void MultiCaptureStartedFromApp(const std::string& label,
                                  const std::string& app_id,
                                  const std::string& app_short_name) override;
  void MultiCaptureStopped(const std::string& label) override;

 private:
  mojo::Remote<video_capture::mojom::MultiCaptureService>
      multi_capture_service_;
  mojo::Receiver<video_capture::mojom::MultiCaptureServiceClient>
      multi_capture_service_observer_receiver_{this};
  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // ASH_MULTI_CAPTURE_MULTI_CAPTURE_SERVICE_CLIENT_H_
