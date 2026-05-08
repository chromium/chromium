// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_PAGE_HANDLER_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_PAGE_HANDLER_H_

#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/token.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/link_to_text/link_to_text.mojom.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace send_tab_to_self {

// Handles the logic for Send Tab to Self on a specific page, including
// capturing page context (e.g. scroll position) and sending the tab to
// the selected device.
class SendTabToSelfPageHandler
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SendTabToSelfPageHandler> {
 public:
  SendTabToSelfPageHandler(const SendTabToSelfPageHandler&) = delete;
  SendTabToSelfPageHandler& operator=(const SendTabToSelfPageHandler&) = delete;

  ~SendTabToSelfPageHandler() override;

  static SendTabToSelfPageHandler* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  // Sends the tab to the target device, potentially after capturing page
  // context (like scroll position).
   void SendTabToDevice(
       const std::string& target_device_guid,
       const GURL& url,
       const std::string& title,
       base::OnceCallback<void(SendTabToSelfResult)> commit_confirmation);

  void SetSelectorGenerationTimeoutForTesting(base::TimeDelta timeout);

 private:
  friend class content::WebContentsUserData<SendTabToSelfPageHandler>;

  explicit SendTabToSelfPageHandler(content::WebContents* web_contents);

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void WebContentsDestroyed() override;

  struct PendingRequest {
    PendingRequest(
        const std::string& target_device_guid,
        const GURL& url,
        const std::string& title,
        base::OnceCallback<void(SendTabToSelfResult)> commit_confirmation);
    PendingRequest(PendingRequest&&);
    PendingRequest& operator=(PendingRequest&&);
    ~PendingRequest();

    std::string target_device_guid;
    GURL url;
    std::string title;
    base::TimeTicks start_time;
    PageContext page_context;
    NavigationHistory navigation_history;
    content::GlobalRenderFrameHostId main_frame_id;
    base::OnceCallback<void(SendTabToSelfResult)> commit_confirmation;
  };

  std::optional<PendingRequest> TakePendingRequest(base::Token request_token);

  void SelectorGeneratedForRequest(
      base::Token request_token,
      const std::string& selector,
      shared_highlighting::LinkGenerationError error,
      shared_highlighting::LinkGenerationReadyStatus ready_status);

  void SelectorGenerationTimedOutForRequest(base::Token request_token);

  void CancelPendingRequests(ScrollPositionGenerationOutcome outcome);

  void RequestScrollPositionSelectorAndSendRequest(base::Token request_token,
                                                   PendingRequest request);

  std::pair<ScrollPositionGenerationOutcome, ScrollPosition>
  ProcessSelectorGenerationResult(
      const PendingRequest& request,
      const std::string& selector,
      shared_highlighting::LinkGenerationError error);

  void MaybeExtractFormFields(PendingRequest& request);

  void MaybeExtractNavigationHistory(PendingRequest& request);

  void SendFinalizedRequest(
      PendingRequest request,
      std::optional<ScrollPositionGenerationOutcome> outcome);

  base::TimeDelta GetSelectorGenerationTimeout() const;

  // The Mojo interface to the text fragment receiver in the renderer. This is
  // used to request the scroll position context from the renderer.
  mojo::Remote<blink::mojom::TextFragmentReceiver> text_fragment_receiver_;

  // The ID of the main frame that the text_fragment_receiver_ is currently
  // bound to.
  content::GlobalRenderFrameHostId last_main_frame_id_;

  // A map of pending requests for text fragment generation. These are stored
  // to be able to associate the response from the renderer with the correct
  // device selection.
  base::flat_map<base::Token, PendingRequest> pending_requests_;

  // Timeout for the renderer to generate a text fragment selector for the
  // viewport center.
  base::TimeDelta selector_generation_timeout_for_testing_;

  base::WeakPtrFactory<SendTabToSelfPageHandler> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_PAGE_HANDLER_H_
