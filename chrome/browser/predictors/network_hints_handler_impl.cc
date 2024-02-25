// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/network_hints_handler_impl.h"

#include <optional>

#include "base/memory/ptr_util.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/preconnect_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/isolation_info.h"

namespace predictors {

namespace {

// Preconnects can be received from the renderer before commit messages, so
// need to use the key from the pending navigation, and not the committed
// navigation, unlike other consumers. This does mean on navigating away from a
// site, preconnect is more likely to incorrectly use the
// NetworkAnonymizationKey of the previous commit.
net::NetworkAnonymizationKey GetPendingNetworkAnonymizationKey(
    content::RenderFrameHost* render_frame_host) {
  return render_frame_host->GetPendingIsolationInfoForSubresources()
      .network_anonymization_key();
}

}  // namespace

NetworkHintsHandlerImpl::~NetworkHintsHandlerImpl() = default;

// static
void NetworkHintsHandlerImpl::Create(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<network_hints::mojom::NetworkHintsHandler> receiver) {
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new NetworkHintsHandlerImpl(frame_host)),
      std::move(receiver));
}

void NetworkHintsHandlerImpl::PrefetchDNS(
    const std::vector<url::SchemeHostPort>& urls) {
  if (!preconnect_manager_)
    return;

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!render_frame_host)
    return;

  std::vector<GURL> gurls;
  for (const auto& url : urls) {
    gurls.emplace_back(url.GetURL());
  }
  preconnect_manager_->StartPreresolveHosts(
      gurls, GetPendingNetworkAnonymizationKey(render_frame_host));
}

void NetworkHintsHandlerImpl::Preconnect(const url::SchemeHostPort& url,
                                         bool allow_credentials) {
  if (!preconnect_manager_)
    return;

  if (url.scheme() != url::kHttpScheme && url.scheme() != url::kHttpsScheme) {
    return;
  }

  // TODO(mmenke):  Think about enabling cross-site preconnects, though that
  // will result in at least some cross-site information leakage.

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!render_frame_host)
    return;

  preconnect_manager_->StartPreconnectUrl(
      url.GetURL(), allow_credentials,
      GetPendingNetworkAnonymizationKey(render_frame_host));
}

NetworkHintsHandlerImpl::NetworkHintsHandlerImpl(
    content::RenderFrameHost* frame_host)
    : render_process_id_(frame_host->GetProcess()->GetID()),
      render_frame_id_(frame_host->GetRoutingID()) {
  // Get the PreconnectManager for this process.
  auto* render_process_host = frame_host->GetProcess();
  auto* profile =
      Profile::FromBrowserContext(render_process_host->GetBrowserContext());
  auto* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile);
  if (loading_predictor && loading_predictor->preconnect_manager())
    preconnect_manager_ = loading_predictor->preconnect_manager()->GetWeakPtr();
}

}  // namespace predictors
