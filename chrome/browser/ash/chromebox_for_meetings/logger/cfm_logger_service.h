// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_LOGGER_CFM_LOGGER_SERVICE_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_LOGGER_CFM_LOGGER_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_observer.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_adaptor.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/meet_devices_logger.mojom-shared.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/meet_devices_logger.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::cfm {

// Implementation of the MeetDevicesLogger Service.
class CfmLoggerService : public CfmObserver,
                         public chromeos::cfm::ServiceAdaptor::Delegate,
                         public chromeos::cfm::mojom::MeetDevicesLogger {
 public:
  class Delegate {
   public:
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    virtual ~Delegate() = default;

    // Perform setup for the delegate,
    // Called after the Logging Service becomes discoverable.
    // May be called more than once but only after a |Reset|
    virtual void Init() = 0;

    // Cleanup the delegate called on mojom connection loss.
    // Called after the Logging Service is no longer discoverable.
    virtual void Reset() = 0;

    // Forwards a request to enqueue a serialised message to be processed.
    virtual void Enqueue(const std::string& record,
                         chromeos::cfm::mojom::EnqueuePriority priority,
                         EnqueueCallback callback) = 0;

   protected:
    Delegate() = default;
  };

  CfmLoggerService(const CfmLoggerService&) = delete;
  CfmLoggerService& operator=(const CfmLoggerService&) = delete;

  // Manage singleton instance.
  // Initialize CfmLoggerService using the ReportingPipeline as the delegate.
  static void Initialize();
  static void InitializeForTesting(Delegate* delegate);
  static void Shutdown();
  static CfmLoggerService* Get();
  static bool IsInitialized();

 protected:
  // If nullptr is passed the default Delegate ReportingPipeline will be used
  explicit CfmLoggerService(Delegate* delegate_);
  ~CfmLoggerService() override;

  // CfmObserver implementation
  bool ServiceRequestReceived(const std::string& interface_name) override;

  // chromeos::cfm::ServiceAdaptor::Delegate implementation
  void OnBindService(mojo::ScopedMessagePipeHandle receiver_pipe) override;
  void OnAdaptorConnect(bool success) override;
  void OnAdaptorDisconnect() override;

  // mojom::MeetDevicesLogger implementation
  void Enqueue(const std::string& record,
               chromeos::cfm::mojom::EnqueuePriority priority,
               EnqueueCallback callback) override;
  void AddStateObserver(
      mojo::PendingRemote<chromeos::cfm::mojom::LoggerStateObserver>
          pending_observer) override;

  // Notifies registered observers of changes in the state of the service.
  void NotifyStateObserver(chromeos::cfm::mojom::LoggerState state);

  // Set the CfmLoggerService::Delegate
  void SetDelegate(Delegate* delegate);

 private:
  raw_ptr<Delegate> delegate_;
  chromeos::cfm::ServiceAdaptor service_adaptor_;
  mojo::ReceiverSet<chromeos::cfm::mojom::MeetDevicesLogger> receivers_;
  chromeos::cfm::mojom::LoggerState current_logger_state_;
  mojo::RemoteSet<chromeos::cfm::mojom::LoggerStateObserver> observer_list_;
};

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_LOGGER_CFM_LOGGER_SERVICE_H_
