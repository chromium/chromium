// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chromebox_for_meetings/browser/cfm_browser_service.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "chrome/browser/memory_details.h"
#include "chromeos/dbus/chromebox_for_meetings/cfm_hotline_client.h"
#include "components/variations/field_trial_config/field_trial_util.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

#include "chrome/browser/chromeos/chromebox_for_meetings/browser/cfm_memory_details.h"

namespace chromeos {
namespace cfm {

namespace {
static CfmBrowserService* g_browser_service = nullptr;
}  // namespace

// static
void CfmBrowserService::Initialize() {
  CHECK(!g_browser_service);
  g_browser_service = new CfmBrowserService();
}

// static
void CfmBrowserService::Shutdown() {
  CHECK(g_browser_service);
  delete g_browser_service;
  g_browser_service = nullptr;
}

// static
CfmBrowserService* CfmBrowserService::Get() {
  CHECK(g_browser_service)
      << "CfmBrowserService::Get() called before Initialize()";
  return g_browser_service;
}

// static
bool CfmBrowserService::IsInitialized() {
  return g_browser_service;
}

bool CfmBrowserService::ServiceRequestReceived(
    const std::string& interface_name) {
  if (interface_name != mojom::CfmBrowser::Name_) {
    return false;
  }
  service_adaptor_.BindServiceAdaptor();
  return true;
}

void CfmBrowserService::OnAdaptorDisconnect() {
  LOG(ERROR) << "mojom::CfmBrowser Service Adaptor has been disconnected";
  // CleanUp to follow the lifecycle of the primary CfmServiceContext
  receivers_.Clear();
}

void CfmBrowserService::OnBindService(
    mojo::ScopedMessagePipeHandle receiver_pipe) {
  receivers_.Add(
      this, mojo::PendingReceiver<mojom::CfmBrowser>(std::move(receiver_pipe)));
}

void CfmBrowserService::GetVariationsData(GetVariationsDataCallback callback) {
  std::string field_trial_parameters =
      base::FieldTrialList::AllParamsToString(true, &variations::EscapeValue);

  std::string field_trial_states;
  base::FieldTrialList::AllStatesToString(&field_trial_states, true);

  std::string enabled_features;
  std::string disabled_features;
  base::FeatureList::GetInstance()->GetFeatureOverrides(&enabled_features,
                                                        &disabled_features);

  std::move(callback).Run(
      std::move(field_trial_parameters), std::move(field_trial_states),
      std::move(enabled_features), std::move(disabled_features));
}

void CfmBrowserService::GetMemoryDetails(GetMemoryDetailsCallback callback) {
  CfmMemoryDetails::Collect(std::move(callback));
}

void CfmBrowserService::OnMojoDisconnect() {
  VLOG(3) << "mojom::CfmBrowser disconnected";
}

// Private methods

CfmBrowserService::CfmBrowserService()
    : service_adaptor_(mojom::CfmBrowser::Name_, this) {
  CfmHotlineClient::Get()->AddObserver(this);

  receivers_.set_disconnect_handler(base::BindRepeating(
      &CfmBrowserService::OnMojoDisconnect, base::Unretained(this)));
}

CfmBrowserService::~CfmBrowserService() {
  CfmHotlineClient::Get()->RemoveObserver(this);
}

}  // namespace cfm
}  // namespace chromeos
