// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_WEB_CONTENTS_TAG_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_WEB_CONTENTS_TAG_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace task_manager {

class RendererTask;
class WebContentsTaskProvider;

// Defines a TaskManager-specific UserData type for WebContents. This is an
// abstract base class for all concrete UserData types. They all share the same
// key. We have a concrete type for each WebContents owning service that the
// task manager is interested in tracking.
//
// To instantiate a |WebContentsTag|, use the factory functions in
// |task_manager::WebContentsTags|.
class WebContentsTag : public base::SupportsUserData::Data {
 public:
  WebContentsTag(const WebContentsTag&) = delete;
  WebContentsTag& operator=(const WebContentsTag&) = delete;
  ~WebContentsTag() override;

  // Retrieves the instance of the WebContentsTag that was attached to the
  // specified WebContents and returns it. If no instance was attached, returns
  // nullptr.
  static const WebContentsTag* FromWebContents(
      const content::WebContents* contents);

  // The concrete Tags know how to instantiate a |RendererTask| that corresponds
  // to the owning WebContents and Service. This will be used by the
  // WebContentsTaskProvider to create the appropriate Tasks. |task_provicer| is
  // provided in case the task needs it for construction.
  virtual std::unique_ptr<RendererTask> CreateTask(
      WebContentsTaskProvider* task_provider) const = 0;

  content::WebContents* web_contents() const { return web_contents_; }

 protected:
  friend class WebContentsTags;

  explicit WebContentsTag(content::WebContents* contents);

 private:
  // The user data key.
  static void* kTagKey;

  // The owning WebContents.
  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_WEB_CONTENTS_TAG_H_
