// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_WEB_CONTENTS_TASK_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_WEB_CONTENTS_TASK_PROVIDER_H_

#include <map>

#include "chrome/browser/task_manager/providers/task_provider.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace task_manager {

class WebContentsTag;

// Defines a provider to provide the renderer tasks that are associated with
// various |WebContents| from various services.
// There should be no or only one instance of this class at any time.
class WebContentsTaskProvider : public TaskProvider {
 public:
  WebContentsTaskProvider();
  WebContentsTaskProvider(const WebContentsTaskProvider&) = delete;
  WebContentsTaskProvider& operator=(const WebContentsTaskProvider&) = delete;
  ~WebContentsTaskProvider() override;

  // This will be called every time we're notified that a new |WebContentsTag|
  // has been created.
  void OnWebContentsTagCreated(const WebContentsTag* tag);

  // Manually remove |tag|'s corresponding Task.
  void OnWebContentsTagRemoved(const WebContentsTag* tag);

  // task_manager::TaskProvider:
  Task* GetTaskOfUrlRequest(int child_id, int route_id) override;

  // Checks if the given |web_contents| is tracked by the provider.
  bool HasWebContents(content::WebContents* web_contents) const;

  // Returns the task, if any, of the provided frame.
  Task* GetTaskOfFrame(content::RenderFrameHost* frame);

 private:
  class WebContentsEntry;

  // task_manager::TaskProvider:
  void StartUpdating() override;
  void StopUpdating() override;

  // Called when the given |web_contents| are destroyed so that we can delete
  // its associated entry.
  void DeleteEntry(content::WebContents* web_contents);

  // A map to associate a |WebContents| with its corresponding entry that we
  // create for it to be able to track it.
  std::map<content::WebContents*, std::unique_ptr<WebContentsEntry>>
      entries_map_;

  // True if this provider is listening to WebContentsTags and updating its
  // observers, false otherwise.
  bool is_updating_ = false;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_WEB_CONTENTS_TASK_PROVIDER_H_
