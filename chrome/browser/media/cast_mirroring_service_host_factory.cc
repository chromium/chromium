// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cast_mirroring_service_host_factory.h"

#include "chrome/browser/media/cast_mirroring_service_host.h"
#include "components/media_router/common/media_source.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"

namespace mirroring {

// static
CastMirroringServiceHostFactory&
CastMirroringServiceHostFactory::GetInstance() {
  static base::NoDestructor<CastMirroringServiceHostFactory> instance;
  return *instance;
}

std::unique_ptr<MirroringServiceHost>
CastMirroringServiceHostFactory::GetForTab(
    content::FrameTreeNodeId frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* target_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (target_contents) {
    const content::DesktopMediaID media_id =
        CastMirroringServiceHost::BuildMediaIdForWebContents(target_contents);
    return std::make_unique<CastMirroringServiceHost>(media_id);
  }
  return nullptr;
}

std::unique_ptr<MirroringServiceHost>
CastMirroringServiceHostFactory::GetForDesktop(
    const std::optional<std::string>& media_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return media_id ? std::make_unique<CastMirroringServiceHost>(
                        content::DesktopMediaID::Parse(*media_id))
                  : nullptr;
}

std::unique_ptr<MirroringServiceHost>
CastMirroringServiceHostFactory::GetForOffscreenTab(
    const GURL& presentation_url,
    const std::string& presentation_id,
    content::FrameTreeNodeId frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (web_contents && media_router::IsValidPresentationUrl(presentation_url)) {
    auto host =
        std::make_unique<CastMirroringServiceHost>(content::DesktopMediaID());
    host->OpenOffscreenTab(web_contents->GetBrowserContext(), presentation_url,
                           presentation_id);
    return host;
  }
  return nullptr;
}

CastMirroringServiceHostFactory::CastMirroringServiceHostFactory() = default;

CastMirroringServiceHostFactory::~CastMirroringServiceHostFactory() = default;

}  // namespace mirroring
