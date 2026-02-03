// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_SIWG_BUTTON_FINDER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_SIWG_BUTTON_FINDER_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "chrome/common/actor.mojom.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "url/gurl.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace actor_login {

class SiwgButtonFinder {
 public:
  explicit SiwgButtonFinder(
      optimization_guide::proto::AnnotatedPageContent page_content);
  ~SiwgButtonFinder();

  SiwgButtonFinder(const SiwgButtonFinder&) = delete;
  SiwgButtonFinder& operator=(const SiwgButtonFinder&) = delete;

  struct SiwgButton {
    SiwgButton();
    SiwgButton(int dom_node_id,
               actor::mojom::ObservedToolTargetPtr observed_target);
    ~SiwgButton();
    SiwgButton(SiwgButton&&);
    SiwgButton& operator=(SiwgButton&&);

    int dom_node_id;
    // Button's observed target, used for the TOCTOU check.
    actor::mojom::ObservedToolTargetPtr observed_target;
  };

  // Returns the most likely interactable SiwG button if found.
  std::optional<SiwgButton> FindButton(
      content::RenderFrameHost* rfh,
      const std::vector<autofill::mojom::SiwgButtonDataPtr>& buttons);

 private:
  void BuildContentNodeMaps(const optimization_guide::proto::ContentNode& node);

  std::optional<int> FindClosestClickableAncestor(
      const optimization_guide::proto::ContentNode& node);

  std::optional<int> FindGoogleSdkButton(
      const GURL& button_frame_url,
      const std::vector<autofill::mojom::SiwgButtonDataPtr>& buttons);

  const optimization_guide::proto::ContentAttributes* GetContentAttributes(
      int dom_node_id) const;

  optimization_guide::proto::AnnotatedPageContent page_content_;
  // Lookup table for the nodes in the page content. The nodes are owned by the
  // `page_content_`.
  base::flat_map<int32_t, raw_ptr<const optimization_guide::proto::ContentNode>>
      dom_node_id_to_content_node_;
  // Lookup table for the parent node of a given node. The nodes are owned by
  // the `page_content_`.
  base::flat_map<raw_ptr<const optimization_guide::proto::ContentNode>,
                 raw_ptr<const optimization_guide::proto::ContentNode>>
      parent_map_;
};

}  // namespace actor_login

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_SIWG_BUTTON_FINDER_H_
