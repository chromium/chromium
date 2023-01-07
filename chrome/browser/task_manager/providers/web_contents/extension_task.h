// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_EXTENSION_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_EXTENSION_TASK_H_

#include <memory>

#include "chrome/browser/task_manager/providers/web_contents/renderer_task.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/common/mojom/view_type.mojom.h"

namespace extensions {
class Extension;
}  // namespace extensions

namespace task_manager {

// Defines a task manager representation for extensions.
class ExtensionTask
    : public RendererTask,
      public extensions::IconImage::Observer {
 public:
  ExtensionTask(content::WebContents* web_contents,
                const extensions::Extension* extension,
                extensions::mojom::ViewType view_type);
  ExtensionTask(const ExtensionTask&) = delete;
  ExtensionTask& operator=(const ExtensionTask&) = delete;
  ~ExtensionTask() override;

  // task_manager::RendererTask
  void UpdateTitle() override;
  void UpdateFavicon() override;
  void Activate() override;
  Type GetType() const override;

  // task_manager::Task
  int GetKeepaliveCount() const override;

  // extensions::IconImage::Observer
  void OnExtensionIconImageChanged(extensions::IconImage* image) override;

 private:
  // If |extension| is nullptr, this method will get the title from
  // the |web_contents|.
  std::u16string GetExtensionTitle(content::WebContents* web_contents,
                                   const extensions::Extension* extension,
                                   extensions::mojom::ViewType view_type) const;

  // This is called upon the creation of this task to load the extension icon
  // for the first time if any.
  void LoadExtensionIcon(const extensions::Extension* extension);

  static gfx::ImageSkia* s_icon_;

  // The favicon of the extension represented by this task.
  std::unique_ptr<extensions::IconImage> extension_icon_;

  const extensions::mojom::ViewType view_type_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_EXTENSION_TASK_H_
