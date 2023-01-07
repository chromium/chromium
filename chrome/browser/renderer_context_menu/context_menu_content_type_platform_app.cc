// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/context_menu_content_type_platform_app.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

using extensions::Extension;
using extensions::ProcessManager;

ContextMenuContentTypePlatformApp::ContextMenuContentTypePlatformApp(
    content::WebContents* web_contents,
    const content::ContextMenuParams& params)
    : ContextMenuContentType(params, false),
      source_web_contents_(web_contents) {}

ContextMenuContentTypePlatformApp::~ContextMenuContentTypePlatformApp() {
}

const Extension* ContextMenuContentTypePlatformApp::GetExtension() {
  ProcessManager* process_manager =
      ProcessManager::Get(source_web_contents_->GetBrowserContext());
  return process_manager->GetExtensionForWebContents(source_web_contents_);
}

bool ContextMenuContentTypePlatformApp::SupportsGroup(int group) {
  const extensions::Extension* platform_app = GetExtension();

  // The RVH might be for a process sandboxed from the extension.
  if (!platform_app)
    return false;

  DCHECK(platform_app->is_platform_app());

  switch (group) {
    // Add undo/redo, cut/copy/paste etc for text fields.
    case ITEM_GROUP_EDITABLE:
    case ITEM_GROUP_COPY:
      return ContextMenuContentType::SupportsGroup(group);
    case ITEM_GROUP_CURRENT_EXTENSION:
      return true;
    case ITEM_GROUP_DEVTOOLS_UNPACKED_EXT:
      // Add dev tools for unpacked extensions.
      return extensions::Manifest::IsUnpackedLocation(
                 platform_app->location()) ||
             base::CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kDebugPackedApps);
    default:
      return false;
  }
}
