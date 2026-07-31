// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EXTENSION_BINDER_PROVIDER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EXTENSION_BINDER_PROVIDER_H_

#include "extensions/browser/extension_mojo_binder_registry.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

namespace contextual_tasks {

// Provides extension-scoped Mojo interface binders for the Contextual Tasks
// component extension (e.g. composebox page handler factory).
class ContextualTasksExtensionBinderProvider
    : public extensions::ExtensionMojoBinderProvider {
 public:
  static void Register(content::BrowserContext* browser_context);

  ContextualTasksExtensionBinderProvider();
  ContextualTasksExtensionBinderProvider(
      const ContextualTasksExtensionBinderProvider&) = delete;
  ContextualTasksExtensionBinderProvider& operator=(
      const ContextualTasksExtensionBinderProvider&) = delete;
  ~ContextualTasksExtensionBinderProvider() override;

  // extensions::ExtensionMojoBinderProvider:
  extensions::ExtensionId GetExtensionId() const override;
  void PopulateFrameBinders(
      mojo::BinderMapWithContext<content::RenderFrameHost*>& binder_map,
      content::RenderFrameHost* render_frame_host,
      const extensions::Extension* extension) override;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EXTENSION_BINDER_PROVIDER_H_
