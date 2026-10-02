// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_HEADLESS_WEB_CONTENTS_MANAGER_H_
#define CHROME_BROWSER_ACTOR_HEADLESS_WEB_CONTENTS_MANAGER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "content/public/browser/web_contents_delegate.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace actor {

// Central class responsible for creating and owning headless WebContents, i.e.
// WebContents that are not attached to a tab or a browser window. Notifies
// observers about the lifecycle of a headless WebContents.
class HeadlessWebContentsManager : public content::WebContentsDelegate {
 public:
  // Observer to be notified about headless WebContents lifecycle events.
  class Observer : public base::CheckedObserver {
   public:
    // Called before `contents` is destroyed, while it is still valid.
    virtual void OnHeadlessContentsWillBeDestroyed(
        content::WebContents* contents) = 0;
  };

  explicit HeadlessWebContentsManager(content::BrowserContext* browser_context);
  HeadlessWebContentsManager(const HeadlessWebContentsManager&) = delete;
  HeadlessWebContentsManager& operator=(const HeadlessWebContentsManager&) =
      delete;
  ~HeadlessWebContentsManager() override;

  // Creates a headless WebContents owned by this manager. The returned pointer
  // stays valid until observers are notified of its destruction.
  content::WebContents* Create();

  // Notifies observers, then destroys `contents`. `contents` must be owned by
  // this manager.
  void Destroy(content::WebContents* contents);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  content::BrowserContext* browser_context() const { return browser_context_; }

  // content::WebContentsDelegate:
  void CloseContents(content::WebContents* source) override;

 private:
  // The context in which all headless WebContents are created.
  const raw_ptr<content::BrowserContext> browser_context_;
  // The headless WebContents owned by this manager.
  std::vector<std::unique_ptr<content::WebContents>> contents_;
  // Observers notified about the lifecycle of the owned WebContents.
  base::ObserverList<Observer> observers_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_HEADLESS_WEB_CONTENTS_MANAGER_H_
