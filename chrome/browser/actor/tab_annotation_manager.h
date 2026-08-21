// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TAB_ANNOTATION_MANAGER_H_
#define CHROME_BROWSER_ACTOR_TAB_ANNOTATION_MANAGER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom.h"

namespace content {
class Page;
class WebContents;
}  // namespace content

namespace actor {

// Manages annotations (text highlight and scroll-to) on a tab's WebContents.
class TabAnnotationManager
    : public content::WebContentsUserData<TabAnnotationManager>,
      public content::WebContentsObserver,
      public blink::mojom::AnnotationAgentHost {
 public:
  TabAnnotationManager(const TabAnnotationManager&) = delete;
  TabAnnotationManager& operator=(const TabAnnotationManager&) = delete;

  ~TabAnnotationManager() override;

  using HighlightCallback = base::OnceCallback<void(bool success)>;

  // Highlights the text matching `query` in the tab's primary main frame and
  // scrolls it into view. If a previous highlight is active, it is
  // removed before creating a new highlight. `callback` is called with true
  // if the text was found and attached, or false if attachment failed.
  void HighlightText(const std::string& query, HighlightCallback callback);

  // Clears any active highlight on the tab.
  void ClearHighlight();

  // Returns true if there is an active highlight or a highlight request in
  // progress.
  bool HasActiveHighlight() const;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  // blink::mojom::AnnotationAgentHost:
  void DidFinishAttachment(
      const gfx::Rect& document_relative_rect,
      blink::mojom::AttachmentResult attachment_result) override;

 private:
  friend class content::WebContentsUserData<TabAnnotationManager>;

  explicit TabAnnotationManager(content::WebContents* web_contents);

  void Reset();
  void OnAgentDisconnected();

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  content::WeakDocumentPtr current_document_;
  mojo::Remote<blink::mojom::AnnotationAgentContainer> annotation_container_;
  mojo::Remote<blink::mojom::AnnotationAgent> annotation_agent_;
  mojo::Receiver<blink::mojom::AnnotationAgentHost> agent_host_receiver_{this};
  HighlightCallback pending_highlight_callback_;

  base::WeakPtrFactory<TabAnnotationManager> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TAB_ANNOTATION_MANAGER_H_
