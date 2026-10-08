// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/network_request_proxying_url_loader_factory.h"

#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/enterprise/connectors/analysis/network_request_content_analysis_info.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/reporting/reporting_event_router_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/network_request_handler.h"
#include "components/enterprise/connectors/core/features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

using ScanCompletedCallback =
    NetworkRequestProxyingURLLoaderFactory::ScanCompletedCallback;
using URLLoaderFactoryType =
    content::ContentBrowserClient::URLLoaderFactoryType;

// Returns the callback set by `SetScanCompletedCallbackForTesting()`.
ScanCompletedCallback& GetScanCompletedCallback() {
  static base::NoDestructor<ScanCompletedCallback> callback;
  return *callback;
}

// Scans the body of a single network request and reports the verdict.
//
// This class owns itself so that the scan can complete even if the proxy that
// started it is destroyed, which happens when the document that made the
// request goes away. It deletes itself once the scan is complete, or when its
// profile is destroyed since it depends on keyed services of that profile.
class NetworkRequestScan : public ProfileObserver {
 public:
  NetworkRequestScan(const NetworkRequestScan&) = delete;
  NetworkRequestScan& operator=(const NetworkRequestScan&) = delete;

  static void Start(Profile* profile,
                    content::WebContents* web_contents,
                    content::GlobalRenderFrameHostId frame_id,
                    const GURL& url,
                    scoped_refptr<network::ResourceRequestBody> body,
                    AnalysisSettings settings) {
    // Deleted by `DeleteSelf()` or `OnProfileWillBeDestroyed()`.
    auto* scan = new NetworkRequestScan(profile, web_contents, frame_id, url,
                                        std::move(body), std::move(settings));
    scan->handler_.UploadData();
  }

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override { delete this; }

 private:
  NetworkRequestScan(Profile* profile,
                     content::WebContents* web_contents,
                     content::GlobalRenderFrameHostId frame_id,
                     const GURL& url,
                     scoped_refptr<network::ResourceRequestBody> body,
                     AnalysisSettings settings)
      : info_(std::move(settings), web_contents, url, frame_id),
        // Unretained is safe since `this` owns `handler_`.
        handler_(&info_,
                 GetBinaryUploadServiceForConnector(profile, info_.settings()),
                 ReportingEventRouterFactory::GetForBrowserContext(profile),
                 url,
                 std::move(body),
                 base::BindOnce(&NetworkRequestScan::OnScanCompleted,
                                base::Unretained(this)),
                 base::BindRepeating(&GetBrowserPolicyConnector)) {
    profile_observation_.Observe(profile);
  }

  ~NetworkRequestScan() override = default;

  void OnScanCompleted(RequestHandlerResult result) {
    if (GetScanCompletedCallback()) {
      GetScanCompletedCallback().Run(result);
    }

    // `this` can't be deleted synchronously since `handler_` is running this
    // callback. A weak pointer is used in case the profile is destroyed before
    // the task runs.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&NetworkRequestScan::DeleteSelf,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  void DeleteSelf() { delete this; }

  // `info_` must outlive `handler_`, which keeps a pointer to it.
  NetworkRequestContentAnalysisInfo info_;
  NetworkRequestHandler handler_;

  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
  base::WeakPtrFactory<NetworkRequestScan> weak_ptr_factory_{this};
};

}  // namespace

NetworkRequestProxyingURLLoaderFactory::NetworkRequestProxyingURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory_remote,
    content::GlobalRenderFrameHostId frame_id,
    base::SelfDeletingPassKey pass_key)
    : network::SelfDeletingURLLoaderFactory(std::move(loader_receiver),
                                            pass_key),
      frame_id_(frame_id) {
  target_factory_.Bind(std::move(target_factory_remote));
  target_factory_.set_disconnect_handler(base::BindOnce(
      &NetworkRequestProxyingURLLoaderFactory::OnTargetFactoryError,
      base::Unretained(this)));
}

NetworkRequestProxyingURLLoaderFactory::
    ~NetworkRequestProxyingURLLoaderFactory() = default;

// static
void NetworkRequestProxyingURLLoaderFactory::MaybeProxyRequest(
    content::RenderFrameHost* frame,
    URLLoaderFactoryType type,
    network::URLLoaderFactoryBuilder& factory_builder) {
  if (!base::FeatureList::IsEnabled(kEnableAuditOnlyNetworkRequestConnector)) {
    return;
  }

  // The policy applies based on the URL of the tab making the request, so
  // only factories used by frames are proxied.
  if (!frame || (type != URLLoaderFactoryType::kNavigation &&
                 type != URLLoaderFactoryType::kDocumentSubResource)) {
    return;
  }

  // Avoid adding a proxy to every factory when the policy isn't set.
  auto* service = ConnectorsServiceFactory::GetForBrowserContext(
      frame->GetBrowserContext());
  if (!service ||
      !service->IsConnectorEnabled(AnalysisConnector::NETWORK_REQUEST)) {
    return;
  }

  auto [receiver, remote] = factory_builder.Append();
  base::MakeSelfDeleting<NetworkRequestProxyingURLLoaderFactory>(
      std::move(receiver), std::move(remote), frame->GetGlobalId());
}

// static
base::AutoReset<ScanCompletedCallback>
NetworkRequestProxyingURLLoaderFactory::SetScanCompletedCallbackForTesting(
    ScanCompletedCallback callback) {
  return base::AutoReset<ScanCompletedCallback>(&GetScanCompletedCallback(),
                                                std::move(callback));
}

void NetworkRequestProxyingURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  // Scans are audit-only, so they never delay the request.
  target_factory_->CreateLoaderAndStart(std::move(loader_receiver), request_id,
                                        options, request, std::move(client),
                                        traffic_annotation);

  MaybeStartScan(request);
}

void NetworkRequestProxyingURLLoaderFactory::MaybeStartScan(
    const network::ResourceRequest& request) {
  if (request.method != net::HttpRequestHeaders::kPostMethod ||
      !request.request_body || request.request_body->elements()->empty() ||
      !request.url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  // The frame might have been destroyed since the proxy was created.
  content::RenderFrameHost* frame = content::RenderFrameHost::FromID(frame_id_);
  if (!frame) {
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame);
  if (!web_contents) {
    return;
  }

  Profile* profile = Profile::FromBrowserContext(frame->GetBrowserContext());
  auto* service = ConnectorsServiceFactory::GetForBrowserContext(profile);
  if (!service) {
    return;
  }

  std::optional<AnalysisSettings> settings =
      service->GetNetworkRequestAnalysisSettings(
          web_contents->GetLastCommittedURL(), request.url);
  if (!settings) {
    return;
  }

  NetworkRequestScan::Start(profile, web_contents, frame_id_, request.url,
                            request.request_body, std::move(*settings));
}

void NetworkRequestProxyingURLLoaderFactory::OnTargetFactoryError() {
  DisconnectReceiversAndDestroy();
}

}  // namespace enterprise_connectors
