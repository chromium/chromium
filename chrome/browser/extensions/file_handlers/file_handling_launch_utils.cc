// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/file_handlers/file_handling_launch_utils.h"

#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/web_applications/web_app_launch_queue.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/web_file_handlers_info.h"

namespace extensions {

void EnqueueLaunchParamsInWebContents(content::WebContents* web_contents,
                                      const Extension& extension,
                                      const GURL& url,
                                      std::vector<base::FilePath> paths) {
  CHECK(extensions::WebFileHandlers::SupportsWebFileHandlers(extension));

  // Enable LaunchQueue in Web file handlers.
  web_app::WebAppLaunchParams launch_params;
  launch_params.started_new_navigation = true;
  launch_params.app_id = extension.id();
  launch_params.target_url = url;
  launch_params.paths = paths;
  web_app::WebAppTabHelper::CreateForWebContents(web_contents);
  web_app::WebAppTabHelper::FromWebContents(web_contents)
      ->EnsureLaunchQueue()
      .Enqueue(launch_params);
}

}  // namespace extensions
