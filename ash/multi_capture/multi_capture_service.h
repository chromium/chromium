// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MULTI_CAPTURE_MULTI_CAPTURE_SERVICE_H_
#define ASH_MULTI_CAPTURE_MULTI_CAPTURE_SERVICE_H_

#include <string>

#include "ash/ash_export.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "url/origin.h"

namespace ash {

// Receives events about multi captures being started / stopped and forwards it
// to ash clients to show usage indicators.
class ASH_EXPORT MultiCaptureService {
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
    virtual void MultiCaptureStartedFromApp(const std::string& label,
                                            const std::string& app_id,
                                            const std::string& app_short_name,
                                            const url::Origin& app_origin) = 0;
    virtual void MultiCaptureStopped(const std::string& label) = 0;
    virtual void MultiCaptureServiceDestroyed() = 0;

   protected:
    ~Observer() override = default;
  };

  MultiCaptureService();
  virtual ~MultiCaptureService();
  MultiCaptureService(const MultiCaptureService&) = delete;
  MultiCaptureService& operator=(const MultiCaptureService&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void NotifyMultiCaptureStarted(const std::string& label,
                                 const url::Origin& origin);
  void NotifyMultiCaptureStartedFromApp(const std::string& label,
                                        const std::string& app_id,
                                        const std::string& app_short_name,
                                        const url::Origin& app_origin);
  void NotifyMultiCaptureStopped(const std::string& label);

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // ASH_MULTI_CAPTURE_MULTI_CAPTURE_SERVICE_H_
