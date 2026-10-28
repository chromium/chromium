// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EXTENSION_ACTOR_TRACKER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EXTENSION_ACTOR_TRACKER_H_

#include <optional>
#include <string_view>

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace enterprise_connectors {

// Tracks active extension actor attribution on a WebContents for enterprise
// security reporting events.
class ExtensionActorTracker
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ExtensionActorTracker> {
 public:
  ~ExtensionActorTracker() override;

  ExtensionActorTracker(const ExtensionActorTracker&) = delete;
  ExtensionActorTracker& operator=(const ExtensionActorTracker&) = delete;

  // Returns true if extension actor attribution is enabled for
  // `browser_context`.
  static bool IsEnabled(content::BrowserContext* browser_context);

  // Marks `web_contents` as acted on by `extension_id` via `command`.
  static void SetActiveActor(content::WebContents* web_contents,
                             const extensions::ExtensionId& extension_id,
                             std::string_view command);

  // Clears active extension actor attribution on `web_contents`.
  static void ClearActiveActor(content::WebContents* web_contents);

  // Returns the active extension ID acting on `web_contents`, if any.
  static std::optional<extensions::ExtensionId> GetActiveActor(
      content::WebContents* web_contents);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidGetUserInteraction(const blink::WebInputEvent& event) override;
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;

 private:
  friend class content::WebContentsUserData<ExtensionActorTracker>;

  explicit ExtensionActorTracker(content::WebContents* web_contents);

  static void SetActiveActorInternal(
      content::WebContents* web_contents,
      const extensions::ExtensionId& extension_id,
      bool is_initial_navigation);

  std::optional<extensions::ExtensionId> active_actor_id_;
  bool is_initial_navigation_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EXTENSION_ACTOR_TRACKER_H_
