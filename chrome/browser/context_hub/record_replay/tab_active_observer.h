// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_TAB_ACTIVE_OBSERVER_H_
#define CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_TAB_ACTIVE_OBSERVER_H_

#include "base/memory/raw_ref.h"
#include "content/public/browser/web_contents_observer.h"

namespace blink {
class WebInputEvent;
}  // namespace blink

namespace content {
class WebContents;
}  // namespace content

namespace context_hub {

class SkillRecordingService;

// Helper class to observe user interactions on the currently active tab's
// `WebContents`.
class TabActiveObserver : public content::WebContentsObserver {
 public:
  // `service` and `web_contents` must be non-null. `service` must outlive this
  // observer.
  TabActiveObserver(SkillRecordingService* service,
                    content::WebContents* web_contents);

  TabActiveObserver(const TabActiveObserver&) = delete;
  TabActiveObserver& operator=(const TabActiveObserver&) = delete;

  ~TabActiveObserver() override;

  // content::WebContentsObserver:
  void DidGetUserInteraction(const blink::WebInputEvent& event) override;

 private:
  const raw_ref<SkillRecordingService> service_;
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_TAB_ACTIVE_OBSERVER_H_
