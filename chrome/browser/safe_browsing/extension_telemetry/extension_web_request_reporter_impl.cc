// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/extension_web_request_reporter_impl.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service_factory.h"
#include "chrome/browser/safe_browsing/extension_telemetry/remote_host_contacted_signal.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom-shared.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

namespace {

// TODO(crbug.com/40937112): Use EnumTraits for these methods.
safe_browsing::RemoteHostInfo::ProtocolType
WebRequestProtocolTypeToRemoteHostInfoProtocolType(
    mojom::WebRequestProtocolType protocol_type) {
  switch (protocol_type) {
    case mojom::WebRequestProtocolType::kHttpHttps:
      return safe_browsing::RemoteHostInfo::HTTP_HTTPS;
    case mojom::WebRequestProtocolType::kWebSocket:
      return safe_browsing::RemoteHostInfo::WEBSOCKET;
  }
}

safe_browsing::RemoteHostInfo::ContactInitiator
WebRequestContactInitatorToRemoteHostInfoContactInitiator(
    mojom::WebRequestContactInitiatorType contact_initiator_type) {
  switch (contact_initiator_type) {
    case mojom::WebRequestContactInitiatorType::kExtension:
      return safe_browsing::RemoteHostInfo::EXTENSION;
    case mojom::WebRequestContactInitiatorType::kContentScript:
      return safe_browsing::RemoteHostInfo::CONTENT_SCRIPT;
  }
}

}  // namespace

const int ExtensionWebRequestReporterImpl::kUserDataKey;

// static
void ExtensionWebRequestReporterImpl::Create(
    content::RenderProcessHost* render_process_host,
    mojo::PendingReceiver<mojom::ExtensionWebRequestReporter> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(render_process_host);

  Profile* profile =
      Profile::FromBrowserContext(render_process_host->GetBrowserContext());
  auto* impl = static_cast<ExtensionWebRequestReporterImpl*>(
      profile->GetUserData(&kUserDataKey));
  if (!impl) {
    auto new_impl = std::make_unique<ExtensionWebRequestReporterImpl>(profile);
    impl = new_impl.get();
    profile->SetUserData(&kUserDataKey, std::move(new_impl));
  }

  impl->Clone(std::move(receiver));
}

ExtensionWebRequestReporterImpl::ExtensionWebRequestReporterImpl(
    Profile* profile)
    : profile_(profile) {
  // It is safe to bind |this| as Unretained because |receivers_| is owned by
  // |this| and will not call this callback after it is destroyed.
  receivers_.set_disconnect_handler(
      base::BindRepeating(&ExtensionWebRequestReporterImpl::OnMojoDisconnect,
                          base::Unretained(this)));
}

ExtensionWebRequestReporterImpl::~ExtensionWebRequestReporterImpl() = default;

void ExtensionWebRequestReporterImpl::SendWebRequestData(
    const std::string& origin_extension_id,
    const GURL& telemetry_url,
    mojom::WebRequestProtocolType protocol_type,
    mojom::WebRequestContactInitiatorType contact_initiator_type) {
  if (protocol_type == mojom::WebRequestProtocolType::kWebSocket) {
    // Logging "true" represents the data being *received*.
    base::UmaHistogramBoolean(
        "SafeBrowsing.ExtensionTelemetry.WebSocketRequestDataSentOrReceived",
        true);
  }
  auto* telemetry_service =
      safe_browsing::ExtensionTelemetryServiceFactory::GetForProfile(profile_);
  if (!telemetry_service || !telemetry_service->enabled() ||
      !base::FeatureList::IsEnabled(
          safe_browsing::
              kExtensionTelemetryInterceptRemoteHostsContactedInRenderer)) {
    return;
  }

  auto remote_host_signal = std::make_unique<RemoteHostContactedSignal>(
      origin_extension_id, telemetry_url,
      WebRequestProtocolTypeToRemoteHostInfoProtocolType(protocol_type),
      WebRequestContactInitatorToRemoteHostInfoContactInitiator(
          contact_initiator_type));
  telemetry_service->AddSignal(std::move(remote_host_signal));
}

void ExtensionWebRequestReporterImpl::Clone(
    mojo::PendingReceiver<mojom::ExtensionWebRequestReporter> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ExtensionWebRequestReporterImpl::OnMojoDisconnect() {
  if (receivers_.empty()) {
    profile_->RemoveUserData(&kUserDataKey);
    // This object is destroyed.
  }
}

}  // namespace safe_browsing
