// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ghost_loader_view.h"

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace contextual_tasks {

ContextualTasksGhostLoaderView::ContextualTasksGhostLoaderView(
    content::BrowserContext* browser_context)
    : views::WebView(browser_context) {
  SetProperty(views::kElementIdentifierKey,
              kContextualTasksGhostLoaderViewElementId);
  LoadInitialURL(GURL(chrome::kChromeUIContextualTasksGhostLoaderURL));
}

ContextualTasksGhostLoaderView::~ContextualTasksGhostLoaderView() = default;

BEGIN_METADATA(ContextualTasksGhostLoaderView)
END_METADATA

}  // namespace contextual_tasks
