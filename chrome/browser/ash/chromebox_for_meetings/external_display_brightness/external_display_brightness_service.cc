// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/external_display_brightness/external_display_brightness_service.h"

#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_hotline_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"

namespace ash::cfm {

namespace {

// TODO(https://crbug.com/1403174): Remove when namespace of mojoms for CfM are
// migarted to ash.
namespace mojom = ::chromeos::cfm::mojom;

static ExternalDisplayBrightnessService* g_external_display_brightness_service =
    nullptr;

}  // namespace

// static
void ExternalDisplayBrightnessService::Initialize() {
  CHECK(!g_external_display_brightness_service);
  g_external_display_brightness_service =
      new ExternalDisplayBrightnessService();
}

// static
void ExternalDisplayBrightnessService::Shutdown() {
  CHECK(g_external_display_brightness_service);
  delete g_external_display_brightness_service;
  g_external_display_brightness_service = nullptr;
}

// static
ExternalDisplayBrightnessService* ExternalDisplayBrightnessService::Get() {
  CHECK(g_external_display_brightness_service)
      << "ExternalDisplayBrightnessService::Get() called before Initialize()";
  return g_external_display_brightness_service;
}

// static
bool ExternalDisplayBrightnessService::IsInitialized() {
  return g_external_display_brightness_service;
}

bool ExternalDisplayBrightnessService::ServiceRequestReceived(
    const std::string& interface_name) {
  if (interface_name != mojom::ExternalDisplayBrightness::Name_) {
    return false;
  }
  service_adaptor_.BindServiceAdaptor();
  return true;
}

void ExternalDisplayBrightnessService::OnBindService(
    mojo::ScopedMessagePipeHandle receiver_pipe) {
  receivers_.Add(this, mojo::PendingReceiver<mojom::ExternalDisplayBrightness>(
                           std::move(receiver_pipe)));
}

void ExternalDisplayBrightnessService::OnAdaptorConnect(bool success) {
  if (success) {
    VLOG(3) << "Adaptor is connected.";
  } else {
    LOG(ERROR) << "Adaptor connection failed.";
  }
}

void ExternalDisplayBrightnessService::OnAdaptorDisconnect() {
  LOG(ERROR) << "mojom::ExternalDisplayBrightness Service Adaptor has been "
                "disconnected";
  // CleanUp to follow the lifecycle of the primary CfmServiceContext
  receivers_.Clear();
}

void ExternalDisplayBrightnessService::SetExternalDisplayALSBrightness(
    bool enabled) {
  chromeos::PowerManagerClient::Get()->SetExternalDisplayALSBrightness(enabled);
}

void ExternalDisplayBrightnessService::OnGetExternalDisplayALSBrightness(
    GetExternalDisplayALSBrightnessCallback callback,
    std::optional<bool> enabled) {
  std::move(callback).Run(enabled.value_or(false));
}

void ExternalDisplayBrightnessService::GetExternalDisplayALSBrightness(
    GetExternalDisplayALSBrightnessCallback callback) {
  chromeos::PowerManagerClient::Get()->GetExternalDisplayALSBrightness(
      base::BindOnce(
          &ExternalDisplayBrightnessService::OnGetExternalDisplayALSBrightness,
          std::move(callback)));
}

void ExternalDisplayBrightnessService::SetExternalDisplayBrightnessPercent(
    double percent) {
  power_manager::SetBacklightBrightnessRequest request;
  request.set_percent(percent);
  chromeos::PowerManagerClient::Get()->SetScreenBrightness(request);
}

void ExternalDisplayBrightnessService::OnGetExternalDisplayBrightnessPercent(
    GetExternalDisplayBrightnessPercentCallback callback,
    std::optional<double> percent) {
  std::move(callback).Run(percent.value_or(-1.0));
}

void ExternalDisplayBrightnessService::GetExternalDisplayBrightnessPercent(
    GetExternalDisplayBrightnessPercentCallback callback) {
  chromeos::PowerManagerClient::Get()->GetScreenBrightnessPercent(
      base::BindOnce(&ExternalDisplayBrightnessService::
                         OnGetExternalDisplayBrightnessPercent,
                     std::move(callback)));
}

void ExternalDisplayBrightnessService::OnMojoDisconnect() {
  VLOG(3) << "mojom::ExternalDisplayBrightness disconnected";
}

ExternalDisplayBrightnessService::ExternalDisplayBrightnessService()
    : service_adaptor_(mojom::ExternalDisplayBrightness::Name_, this) {
  CfmHotlineClient::Get()->AddObserver(this);

  receivers_.set_disconnect_handler(
      base::BindRepeating(&ExternalDisplayBrightnessService::OnMojoDisconnect,
                          base::Unretained(this)));
}

ExternalDisplayBrightnessService::~ExternalDisplayBrightnessService() {
  CfmHotlineClient::Get()->RemoveObserver(this);
}

}  // namespace ash::cfm
