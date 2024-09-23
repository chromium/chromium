// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CAST_MIRRORING_SERVICE_HOST_FACTORY_H_
#define CHROME_BROWSER_MEDIA_CAST_MIRRORING_SERVICE_HOST_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/media/mirroring_service_host.h"
#include "content/public/browser/frame_tree_node_id.h"

namespace mirroring {

class CastMirroringServiceHostFactory : public MirroringServiceHostFactory {
 public:
  static CastMirroringServiceHostFactory& GetInstance();

  CastMirroringServiceHostFactory(const CastMirroringServiceHostFactory&) =
      delete;
  CastMirroringServiceHostFactory& operator=(
      const CastMirroringServiceHostFactory&) = delete;

  // Returns CastMirroringServiceHost instance if there exist a WebContents
  // connected to `frame_tree_node_id`, otherwise returns nullptr.
  std::unique_ptr<MirroringServiceHost> GetForTab(
      content::FrameTreeNodeId frame_tree_node_id) override;

  // Returns CastMirroringServiceHost instance if `media_id` has a value,
  // otherwise returns nullptr.
  std::unique_ptr<MirroringServiceHost> GetForDesktop(
      const std::optional<std::string>& media_id) override;

  // Returns CastMirroringServiceHost instance if there exist a WebContents
  // connected to `frame_tree_node_id` and `presentation_url` is a valid URL,
  // otherwise returns nullptr.
  std::unique_ptr<MirroringServiceHost> GetForOffscreenTab(
      const GURL& presentation_url,
      const std::string& presentation_id,
      content::FrameTreeNodeId frame_tree_node_id) override;

 private:
  friend class base::NoDestructor<CastMirroringServiceHostFactory>;

  CastMirroringServiceHostFactory();
  ~CastMirroringServiceHostFactory() override;
};

}  // namespace mirroring

#endif  // CHROME_BROWSER_MEDIA_CAST_MIRRORING_SERVICE_HOST_FACTORY_H_
