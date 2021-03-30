// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_EXTENSION_TAG_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_EXTENSION_TAG_H_

#include "base/macros.h"
#include "chrome/browser/task_manager/providers/web_contents/extension_task.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tag.h"
#include "extensions/common/mojom/view_type.mojom.h"

namespace task_manager {

// Defines a concrete UserData type for WebContents owned by extensions.
class ExtensionTag : public WebContentsTag {
 public:
  ~ExtensionTag() override;

  // task_manager::WebContentsTag:
  std::unique_ptr<RendererTask> CreateTask(
      WebContentsTaskProvider*) const override;

 private:
  friend class WebContentsTags;

  ExtensionTag(content::WebContents* web_contents,
               const extensions::mojom::ViewType view_type);

  // The ViewType of the extension WebContents this tag is attached to.
  const extensions::mojom::ViewType view_type_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionTag);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_EXTENSION_TAG_H_
