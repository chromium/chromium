// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_PAGE_METADATA_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_PAGE_METADATA_MANAGER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/blink/public/mojom/page/page.mojom-forward.h"

namespace content {
class WebContents;
}

namespace optimization_guide {
class PageContentMetadataObserver;
}

#include "components/tabs/public/tab_interface.h"

namespace glic {

// Manages subscriptions to page metadata for tabs on behalf of the Glic web
// client. It observes tabs for metadata changes and forwards them to the
// client. It also handles caching of metadata when the Glic panel is inactive.
class PageMetadataManager {
 public:
  explicit PageMetadataManager(glic::mojom::WebClient* web_client);
  ~PageMetadataManager();

  PageMetadataManager(const PageMetadataManager&) = delete;
  PageMetadataManager& operator=(const PageMetadataManager&) = delete;

  // Subscribes to page metadata updates for the given `tab_id`. If `names` is
  // empty, this will unsubscribe from any existing subscription.
  void SubscribeToPageMetadata(
      int32_t tab_id,
      const std::vector<std::string>& names,
      glic::mojom::WebClientHandler::SubscribeToPageMetadataCallback callback);

 private:
  struct PageMetadataSubscription;

  void OnTabWillDiscardContents(tabs::TabInterface* tab,
                                content::WebContents* old_contents,
                                content::WebContents* new_contents);
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);
  void NotifyPageMetadataChanged(int32_t tab_id,
                                 blink::mojom::PageMetadataPtr page_metadata);

  // Unowned. The client is owned by the owner of this PageMetadataManager.
  const raw_ptr<glic::mojom::WebClient> web_client_;

  absl::flat_hash_map<int32_t, PageMetadataSubscription>
      tab_id_to_page_metadata_subscriptions_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_PAGE_METADATA_MANAGER_H_
