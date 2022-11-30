// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/ash_tracing_manager.h"

#include <vector>

#include "ash/hud_display/ash_tracing_request.h"
#include "ash/session/session_controller_impl.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"

namespace ash {
namespace hud_display {

AshTracingManager::AshTracingManager() {
  SessionController::Get()->AddObserver(this);
}

AshTracingManager::~AshTracingManager() {
  if (SessionController::Get())
    SessionController::Get()->RemoveObserver(this);
}

// static
AshTracingManager& AshTracingManager::Get() {
  static base::NoDestructor<AshTracingManager> manager;
  return *manager;
}

bool AshTracingManager::IsBusy() const {
  if (tracing_requests_.empty())
    return false;

  switch (GetLastRequestStatus()) {
    case AshTracingRequest::Status::kEmpty:
      [[fallthrough]];
    case AshTracingRequest::Status::kInitialized:
      return true;
    case AshTracingRequest::Status::kStarted:
      return false;
    case AshTracingRequest::Status::kStopping:
      return true;
    case AshTracingRequest::Status::kPendingMount:
      [[fallthrough]];
    case AshTracingRequest::Status::kWritingFile:
      [[fallthrough]];
    case AshTracingRequest::Status::kCompleted:
      return false;
  }
}

bool AshTracingManager::IsTracingStarted() const {
  if (tracing_requests_.empty())
    return false;

  switch (GetLastRequestStatus()) {
    case AshTracingRequest::Status::kEmpty:
      [[fallthrough]];
    case AshTracingRequest::Status::kInitialized:
      [[fallthrough]];
    case AshTracingRequest::Status::kStarted:
      [[fallthrough]];
    case AshTracingRequest::Status::kStopping:
      return true;
    case AshTracingRequest::Status::kPendingMount:
      [[fallthrough]];
    case AshTracingRequest::Status::kWritingFile:
      [[fallthrough]];
    case AshTracingRequest::Status::kCompleted:
      return false;
  }
}

std::string AshTracingManager::GetStatusMessage() const {
  std::string result;
  if (tracing_requests_.empty())
    return result;

  unsigned started = 0;
  unsigned waiting_for_login = 0;
  unsigned writing_trace_file = 0;
  unsigned completed = 0;
  for (const auto& request : tracing_requests_) {
    switch (request->status()) {
      case AshTracingRequest::Status::kEmpty:
        [[fallthrough]];
      case AshTracingRequest::Status::kInitialized:
        [[fallthrough]];
      case AshTracingRequest::Status::kStarted:
        [[fallthrough]];
      case AshTracingRequest::Status::kStopping:
        ++started;
        break;
      case AshTracingRequest::Status::kPendingMount:
        ++waiting_for_login;
        break;
      case AshTracingRequest::Status::kWritingFile:
        ++writing_trace_file;
        break;
      case AshTracingRequest::Status::kCompleted:
        ++completed;
    }
  }
  if (started)
    result = base::StringPrintf("%u active", started);

  if (waiting_for_login) {
    if (!result.empty())
      result += ", ";

    result += base::StringPrintf("%u pending login", waiting_for_login);
  }
  if (writing_trace_file) {
    if (!result.empty())
      result += ", ";

    result += base::StringPrintf("%u writing", writing_trace_file);
  }
  if (completed) {
    if (!result.empty())
      result += ", ";

    result += base::StringPrintf("%u completed", completed);
  }
  if (!result.empty())
    result = std::string("Tracing: ") + result + ".";

  return result;
}

void AshTracingManager::Start() {
  DCHECK(!IsBusy());
  DCHECK(!IsTracingStarted());
  tracing_requests_.push_back(std::make_unique<AshTracingRequest>(this));
}

void AshTracingManager::Stop() {
  DCHECK(!tracing_requests_.empty());
  DCHECK_EQ(GetLastRequestStatus(), AshTracingRequest::Status::kStarted);
  tracing_requests_.back()->Stop();
}

void AshTracingManager::AddObserver(AshTracingManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void AshTracingManager::RemoveObserver(AshTracingManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AshTracingManager::OnRequestStatusChanged(AshTracingRequest* request) {
  for (Observer& observer : observers_)
    observer.OnTracingStatusChange();
}

void AshTracingManager::OnFirstSessionStarted() {
  for (auto& request : tracing_requests_)
    request->OnUserLoggedIn();
}

const std::vector<std::unique_ptr<AshTracingRequest>>&
AshTracingManager::GetTracingRequestsForTesting() const {
  return tracing_requests_;
}

AshTracingRequest::Status AshTracingManager::GetLastRequestStatus() const {
  return tracing_requests_.back()->status();
}

}  // namespace hud_display
}  // namespace ash
