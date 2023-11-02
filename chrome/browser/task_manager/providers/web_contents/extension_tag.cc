// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/extension_tag.h"

#include <memory>

#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/process_manager.h"
#endif

namespace task_manager {

std::unique_ptr<RendererTask> ExtensionTag::CreateTask(
    WebContentsTaskProvider*) const {
  // Upon being asked to create a task, it means that the site instance is ready
  // and connected, and the render frames have been initialized.
  // It's OK if the following returns nullptr, ExtensionTask will then get the
  // title from the WebContents.
  const extensions::Extension* extension =
      extensions::ProcessManager::Get(web_contents()->GetBrowserContext())->
          GetExtensionForWebContents(web_contents());

  return std::make_unique<ExtensionTask>(web_contents(), extension, view_type_);
}

ExtensionTag::ExtensionTag(content::WebContents* web_contents,
                           const extensions::mojom::ViewType view_type)
    : WebContentsTag(web_contents), view_type_(view_type) {}

ExtensionTag::~ExtensionTag() {
}

}  // namespace task_manager
