// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chromebox_for_meetings/logger/cfm_logger_service.h"

#include "base/bind.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/chromebox_for_meetings/logger/reporting_pipeline.h"
#include "chromeos/components/chromebox_for_meetings/features/features.h"
#include "chromeos/dbus/chromebox_for_meetings/cfm_hotline_client.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/meet_devices_logger.mojom-shared.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {
namespace cfm {

namespace {

// Implementation of the CfmLoggerService which uses the Chrome Encrypted
// Reporting Pipeline APIs to define the delegate's functionality
class CfmERPLoggerService : public CfmLoggerService {
 public:
  CfmERPLoggerService() : CfmLoggerService(nullptr) {
    reporting_pipeline_ = std::make_unique<ReportingPipeline>(
        base::BindRepeating(&CfmERPLoggerService::NotifyStateObserver,
                            weak_ptr_factory_.GetWeakPtr()));
    SetDelegate(reporting_pipeline_.get());
  }
  CfmERPLoggerService(const CfmERPLoggerService&) = delete;
  CfmERPLoggerService& operator=(const CfmERPLoggerService&) = delete;
  ~CfmERPLoggerService() override = default;

 private:
  std::unique_ptr<Delegate> reporting_pipeline_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CfmERPLoggerService> weak_ptr_factory_{this};
};

static CfmLoggerService* g_logger_service = nullptr;

}  // namespace

// static
void CfmLoggerService::Initialize() {
  CHECK(!g_logger_service);
  g_logger_service = new CfmERPLoggerService();
}

// static
void CfmLoggerService::InitializeForTesting(Delegate* delegate) {
  CHECK(!g_logger_service);
  g_logger_service = new CfmLoggerService(delegate);
}

// static
void CfmLoggerService::Shutdown() {
  CHECK(g_logger_service);
  delete g_logger_service;
  g_logger_service = nullptr;
}

// static
CfmLoggerService* CfmLoggerService::Get() {
  CHECK(g_logger_service)
      << "CfmLoggerService::Get() called before Initialize()";
  return g_logger_service;
}

// static
bool CfmLoggerService::IsInitialized() {
  return g_logger_service;
}

bool CfmLoggerService::ServiceRequestReceived(
    const std::string& interface_name) {
  // If Disabled should not be discoverable
  if (!base::FeatureList::IsEnabled(features::kCloudLogger) ||
      interface_name != mojom::MeetDevicesLogger::Name_) {
    return false;
  }
  service_adaptor_.BindServiceAdaptor();
  return true;
}

void CfmLoggerService::OnBindService(
    mojo::ScopedMessagePipeHandle receiver_pipe) {
  receivers_.Add(this, mojo::PendingReceiver<mojom::MeetDevicesLogger>(
                           std::move(receiver_pipe)));
}

void CfmLoggerService::OnAdaptorConnect(bool success) {
  if (success) {
    VLOG(3) << "Adaptor is connected.";
    delegate_->Init();
  } else {
    LOG(ERROR) << "Adaptor connection failed.";
  }
}

void CfmLoggerService::OnAdaptorDisconnect() {
  LOG(ERROR)
      << "mojom::MeetDevicesLogger Service Adaptor has been disconnected";
  // CleanUp to follow the lifecycle of the primary CfmServiceContext
  receivers_.Clear();
  delegate_->Reset();
}

void CfmLoggerService::Enqueue(const std::string& record,
                               mojom::EnqueuePriority priority,
                               EnqueueCallback callback) {
  delegate_->Enqueue(std::move(record), std::move(priority),
                     std::move(callback));
}

void CfmLoggerService::AddStateObserver(
    mojo::PendingRemote<mojom::LoggerStateObserver> pending_observer) {
  mojo::Remote<mojom::LoggerStateObserver> observer(
      std::move(pending_observer));
  observer->OnNotifyState(current_logger_state_);
  observer_list_.Add(std::move(observer));
}

void CfmLoggerService::NotifyStateObserver(mojom::LoggerState state) {
  current_logger_state_ = state;
  for (auto& observer : observer_list_)
    observer->OnNotifyState(current_logger_state_);
}

void CfmLoggerService::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

// private

CfmLoggerService::CfmLoggerService(Delegate* delegate)
    : delegate_(delegate),
      service_adaptor_(mojom::MeetDevicesLogger::Name_, this),
      current_logger_state_(mojom::LoggerState::kUninitialized) {
  CfmHotlineClient::Get()->AddObserver(this);
}

CfmLoggerService::~CfmLoggerService() {
  CfmHotlineClient::Get()->RemoveObserver(this);
}

}  // namespace cfm
}  // namespace chromeos
