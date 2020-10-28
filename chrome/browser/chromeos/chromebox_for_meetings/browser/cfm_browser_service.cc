// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chromebox_for_meetings/browser/cfm_browser_service.h"

#include "base/bind.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {
namespace cfm {

CfmBrowserService::CfmBrowserService()
    : service_adaptor_(mojom::CfmBrowser::Name_, this) {
  receivers_.set_disconnect_handler(base::BindRepeating(
      &CfmBrowserService::OnMojoDisconnect, base::Unretained(this)));
}

CfmBrowserService::~CfmBrowserService() = default;

bool CfmBrowserService::ServiceRequestReceived(const std::string& service_id) {
  if (service_id != mojom::CfmBrowser::Name_) {
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

void CfmBrowserService::OnMojoDisconnect() {
  VLOG(3) << "mojom::CfmBrowser disconnected";
}

CfmBrowserService* CfmBrowserService::GetInstance() {
  static base::NoDestructor<CfmBrowserService> cfm_browser_service;

  return cfm_browser_service.get();
}

}  // namespace cfm
}  // namespace chromeos
