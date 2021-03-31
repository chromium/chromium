// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_redirect/subresource_redirect_observer.h"

#include "chrome/browser/login_detection/login_detection_keyed_service.h"
#include "chrome/browser/login_detection/login_detection_keyed_service_factory.h"
#include "chrome/browser/login_detection/login_detection_type.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_redirect/origin_robots_rules_cache.h"
#include "chrome/browser/subresource_redirect/subresource_redirect_util.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/optimization_guide/proto/public_image_metadata.pb.h"
#include "components/subresource_redirect/common/subresource_redirect_features.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/gurl.h"

namespace subresource_redirect {

namespace {

// Returns the OptimizationGuideDecider when LiteMode and the subresource
// redirect feature are enabled.
optimization_guide::OptimizationGuideDecider*
GetOptimizationGuideDeciderFromWebContents(content::WebContents* web_contents) {
  DCHECK(ShouldEnablePublicImageHintsBasedCompression());
  if (!web_contents)
    return nullptr;

  if (Profile* profile =
          Profile::FromBrowserContext(web_contents->GetBrowserContext())) {
    if (data_reduction_proxy::DataReductionProxySettings::
            IsDataSaverEnabledByUser(profile->IsOffTheRecord(),
                                     profile->GetPrefs())) {
      return OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
    }
  }
  return nullptr;
}

// Pass down the |images_hints| to |render_frame_host|.
void SetResourceLoadingImageHints(
    content::RenderFrameHost* render_frame_host,
    mojom::CompressPublicImagesHintsPtr images_hints) {
  mojo::AssociatedRemote<mojom::SubresourceRedirectHintsReceiver>
      hints_receiver;
  DCHECK(ShouldEnablePublicImageHintsBasedCompression());

  if (render_frame_host->GetRemoteAssociatedInterfaces()) {
    render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
        &hints_receiver);
    hints_receiver->SetCompressPublicImagesHints(std::move(images_hints));
  }
}

void UpdateRobotsRules(
    mojom::SubresourceRedirectService::GetRobotsRulesCallback callback,
    base::Optional<std::string> robots_rules_proto) {
  std::move(callback).Run(robots_rules_proto);
}

}  // namespace

ImageCompressionAppliedDocument::ImageCompressionAppliedDocument(
    content::RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {
  DCHECK(
      login_detection::LoginDetectionKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(render_frame_host->GetBrowserContext()))
          ->GetPersistentLoginDetection(
              render_frame_host->GetLastCommittedURL()) ==
      login_detection::LoginDetectionType::kNoLogin);
}

ImageCompressionAppliedDocument::~ImageCompressionAppliedDocument() = default;

RENDER_DOCUMENT_HOST_USER_DATA_KEY_IMPL(ImageCompressionAppliedDocument)

void ImageCompressionAppliedDocument::GetAndUpdateRobotsRules(
    const url::Origin& origin,
    OriginRobotsRulesCache* rules_cache,
    mojom::SubresourceRedirectService::GetRobotsRulesCallback callback) {
  if (!rules_cache) {
    std::move(callback).Run(base::nullopt);
    return;
  }
  rules_cache->GetRobotsRules(
      origin, base::BindOnce(&UpdateRobotsRules, std::move(callback)));
}

// static
void SubresourceRedirectObserver::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  if ((ShouldEnablePublicImageHintsBasedCompression() ||
       ShouldEnableRobotsRulesFetching()) &&
      IsLiteModeEnabled(web_contents)) {
    SubresourceRedirectObserver::CreateForWebContents(web_contents);
  }
}

// static
bool SubresourceRedirectObserver::IsHttpsImageCompressionApplied(
    content::WebContents* web_contents) {
  if (!ShouldCompressRedirectSubresource())
    return false;

  SubresourceRedirectObserver* observer =
      SubresourceRedirectObserver::FromWebContents(web_contents);
  return observer && observer->is_mainframe_https_image_compression_applied_;
}

SubresourceRedirectObserver::SubresourceRedirectObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      receivers_(web_contents, this) {
  DCHECK(ShouldEnablePublicImageHintsBasedCompression() ||
         ShouldEnableRobotsRulesFetching());
  if (ShouldEnablePublicImageHintsBasedCompression()) {
    if (auto* optimization_guide_decider =
            GetOptimizationGuideDeciderFromWebContents(web_contents)) {
      optimization_guide_decider->RegisterOptimizationTypes(
          {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});
    }
  }
}

SubresourceRedirectObserver::~SubresourceRedirectObserver() = default;

void SubresourceRedirectObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(navigation_handle);
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->GetRenderFrameHost()) {
    return;
  }
  if (!navigation_handle->IsInMainFrame() &&
      !ShouldEnableRobotsRulesFetching()) {
    return;
  }
  if (!IsLiteModeEnabled(web_contents()))
    return;

  // Set to disable compression by default for the mainframe navigation.
  if (navigation_handle->IsInMainFrame())
    is_mainframe_https_image_compression_applied_ = false;

  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS())
    return;

  // Check and show the one-time infobar when image compression is enabled. This
  // needs to be done for src video compressed navigations too when that gets
  // enabled.
  if ((ShouldEnablePublicImageHintsBasedCompression() ||
       ShouldEnableLoginRobotsCheckedImageCompression()) &&
      !ShowInfoBarAndGetImageCompressionState(web_contents(),
                                              navigation_handle)) {
    return;
  }
  content::RenderFrameHost* render_frame_host =
      navigation_handle->GetRenderFrameHost();

  // Handle login robots based compression mode.
  if (ShouldEnableRobotsRulesFetching()) {
    mojo::AssociatedRemote<mojom::SubresourceRedirectHintsReceiver>
        hints_receiver;
    render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
        &hints_receiver);
    bool is_compression_allowed =
        IsAllowedForCurrentLoginState(navigation_handle);
    hints_receiver->SetLoggedInState(!is_compression_allowed);
    if (ShouldEnableLoginRobotsCheckedImageCompression() &&
        navigation_handle->IsInMainFrame()) {
      is_mainframe_https_image_compression_applied_ = is_compression_allowed;
    }

    if (!is_compression_allowed)
      return;

    // Create the ImageCompressionAppliedDocument only when compression is
    // allowed.
    ImageCompressionAppliedDocument::GetOrCreateForCurrentDocument(
        navigation_handle->GetRenderFrameHost());
    return;
  }

  // Handle public image hints based compression mode.
  DCHECK(ShouldEnablePublicImageHintsBasedCompression());

  auto* optimization_guide_decider = GetOptimizationGuideDeciderFromWebContents(
      navigation_handle->GetWebContents());
  if (!optimization_guide_decider)
    return;

  optimization_guide_decider->CanApplyOptimizationAsync(
      navigation_handle, optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce(
          &SubresourceRedirectObserver::OnResourceLoadingImageHintsReceived,
          weak_factory_.GetWeakPtr(),
          content::GlobalFrameRoutingId(
              render_frame_host->GetProcess()->GetID(),
              render_frame_host->GetRoutingID())));
}

void SubresourceRedirectObserver::OnResourceLoadingImageHintsReceived(
    content::GlobalFrameRoutingId render_frame_host_routing_id,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& optimization_metadata) {
  DCHECK(ShouldEnablePublicImageHintsBasedCompression());

  // Clear |is_mainframe_https_image_compression_applied_| since it may be set
  // to true when multiple navigations are starting and image hints is received
  // for the first one.
  is_mainframe_https_image_compression_applied_ = false;

  content::RenderFrameHost* current_render_frame_host =
      content::RenderFrameHost::FromID(render_frame_host_routing_id);
  // Check if the same render frame host is still valid.
  if (!current_render_frame_host)
    return;

  if (decision != optimization_guide::OptimizationGuideDecision::kTrue)
    return;
  if (!optimization_metadata.public_image_metadata())
    return;

  std::vector<std::string> public_image_urls;
  const optimization_guide::proto::PublicImageMetadata public_image_metadata =
      optimization_metadata.public_image_metadata().value();
  public_image_urls.reserve(public_image_metadata.url_size());
  for (const auto& url : public_image_metadata.url())
    public_image_urls.push_back(url);

  // Pass down the image URLs to renderer even if it could be empty. This acts
  // as a signal that the image hint fetch has finished, for coverage metrics
  // purposes.
  SetResourceLoadingImageHints(
      current_render_frame_host,
      mojom::CompressPublicImagesHints::New(public_image_urls));
  if (!public_image_urls.empty())
    is_mainframe_https_image_compression_applied_ = true;
}

void SubresourceRedirectObserver::NotifyCompressedImageFetchFailed(
    base::TimeDelta retry_after) {
  subresource_redirect::NotifyCompressedImageFetchFailed(web_contents(),
                                                         retry_after);
}

void SubresourceRedirectObserver::GetRobotsRules(
    const url::Origin& origin,
    mojom::SubresourceRedirectService::GetRobotsRulesCallback callback) {
  DCHECK(ShouldEnableRobotsRulesFetching());
  DCHECK(!origin.opaque());
  if (!web_contents()) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  // ImageCompressionAppliedDocument could be null when suresource redirect is
  // disabled for this document.
  auto* subresource_redirect_document_host =
      ImageCompressionAppliedDocument::GetForCurrentDocument(
          web_contents()->GetMainFrame());
  if (!subresource_redirect_document_host) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  subresource_redirect_document_host->GetAndUpdateRobotsRules(
      origin, GetOriginRobotsRulesCache(web_contents()), std::move(callback));
}

bool SubresourceRedirectObserver::IsAllowedForCurrentLoginState(
    content::NavigationHandle* navigation_handle) {
  DCHECK(ShouldEnableRobotsRulesFetching());

  auto* login_detection_keyed_service =
      login_detection::LoginDetectionKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  if (!login_detection_keyed_service)
    return false;

  if (login_detection_keyed_service->GetPersistentLoginDetection(
          navigation_handle->GetURL()) !=
      login_detection::LoginDetectionType::kNoLogin) {
    return false;
  }

  // Check if any of the parent frames have disabled image compression.
  content::RenderFrameHost* parent_render_frame_host =
      navigation_handle->GetRenderFrameHost();
  while ((parent_render_frame_host = parent_render_frame_host->GetParent())) {
    if (!parent_render_frame_host->IsCurrent())
      continue;
    // Existence of ImageCompressionAppliedDocument for the parent render frame
    // indicates the parent is not logged-in and allowed fo subresource
    // redirect.
    if (!ImageCompressionAppliedDocument::GetForCurrentDocument(
            parent_render_frame_host)) {
      return false;
    }
  }
  return true;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SubresourceRedirectObserver)

}  // namespace subresource_redirect
