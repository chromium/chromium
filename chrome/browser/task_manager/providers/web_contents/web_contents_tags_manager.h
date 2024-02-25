// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_WEB_CONTENTS_TAGS_MANAGER_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_WEB_CONTENTS_TAGS_MANAGER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tag.h"

namespace base {
template<typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace task_manager {

class WebContentsTaskProvider;

// Defines a manager to track the various TaskManager-specific WebContents
// UserData (task_manager::WebContentsTags). This is used by the
// WebContentsTaskProvider to get all the pre-existing WebContents once
// WebContentsTaskProvider::StartUpdating() is called.
class WebContentsTagsManager {
 public:
  WebContentsTagsManager(const WebContentsTagsManager&) = delete;
  WebContentsTagsManager& operator=(const WebContentsTagsManager&) = delete;

  static WebContentsTagsManager* GetInstance();

  void AddTag(WebContentsTag* tag);
  void RemoveTag(WebContentsTag* tag);

  // This is how the WebContentsTaskProvider starts and stops observing the
  // creation of WebContents.
  // There must be no or only one given provider at any given time.
  void SetProvider(WebContentsTaskProvider* provider);
  void ClearProvider();

  // This is called by WebContentsTags::ClearTag(). This is needed for Tags
  // whose destruction does not correspond to the destruction of their
  // WebContents. In this case the provider (if any) must be manually cleared,
  // or else the corresponding task for the |tag| will continue to exist.
  void ClearFromProvider(const WebContentsTag* tag);

  const std::vector<raw_ptr<WebContentsTag, VectorExperimental>>& tracked_tags()
      const {
    return tracked_tags_;
  }

 private:
  friend struct base::DefaultSingletonTraits<WebContentsTagsManager>;

  WebContentsTagsManager();
  ~WebContentsTagsManager();

  // The provider that's currently observing the creation of WebContents.
  raw_ptr<WebContentsTaskProvider> provider_;

  // A set of all the WebContentsTags seen so far.
  std::vector<raw_ptr<WebContentsTag, VectorExperimental>> tracked_tags_;
};

}  // namespace task_manager


#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_WEB_CONTENTS_TAGS_MANAGER_H_
