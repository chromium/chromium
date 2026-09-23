// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_post_rearchitecture.h"

#include "content/public/browser/web_ui.h"

namespace contextual_tasks {

ContextualTasksUIPostRearchitecture::ContextualTasksUIPostRearchitecture(
    content::WebUI* web_ui)
    : ContextualTasksUIBase(web_ui) {
  RegisterWebUIDataSource(GetProfile());
}

ContextualTasksUIPostRearchitecture::~ContextualTasksUIPostRearchitecture() =
    default;

WEB_UI_CONTROLLER_TYPE_IMPL(ContextualTasksUIPostRearchitecture)

}  // namespace contextual_tasks
