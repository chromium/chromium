// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_ASH_TRACING_MANAGER_H_
#define ASH_HUD_DISPLAY_ASH_TRACING_MANAGER_H_

#include <memory>

#include "ash/hud_display/ash_tracing_request.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace ash {
namespace hud_display {

// Singleton object to manager Ash tracing sessions.
class ASH_EXPORT AshTracingManager : public SessionObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    ~Observer() override = default;

    virtual void OnTracingStatusChange() = 0;
  };

  AshTracingManager();
  AshTracingManager(const AshTracingManager&) = delete;
  AshTracingManager& operator=(const AshTracingManager&) = delete;
  ~AshTracingManager() override;

  static AshTracingManager& Get();

  // True when tracing is being started or stopped. No control requests are
  // possible.
  bool IsBusy() const;

  // True when tracing can be stopped.
  bool IsTracingStarted() const;

  // Returns user status message.
  std::string GetStatusMessage() const;

  // Initiates asynchronous tracing start. Observer will be notified with the
  // result.
  void Start();

  // Initiates asynchronous tracing stop. Observer will be notified with the
  // result.
  void Stop();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Called by AshTracingRequest to update system state.
  void OnRequestStatusChanged(AshTracingRequest* request);

  // SessionObserver
  void OnFirstSessionStarted() override;

  const std::vector<std::unique_ptr<AshTracingRequest>>&
  GetTracingRequestsForTesting() const;

 private:
  // Returns status of the last request in tracing_requests_.
  AshTracingRequest::Status GetLastRequestStatus() const;

  // Only last tracing request can be active. Other can be either finished, or
  // waiting for user session start to save the trace.
  std::vector<std::unique_ptr<AshTracingRequest>> tracing_requests_;

  base::ObserverList<Observer> observers_;
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_ASH_TRACING_MANAGER_H_
