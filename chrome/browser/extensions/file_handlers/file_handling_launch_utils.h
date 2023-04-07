// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FILE_HANDLERS_FILE_HANDLING_LAUNCH_UTILS_H_
#define CHROME_BROWSER_EXTENSIONS_FILE_HANDLERS_FILE_HANDLING_LAUNCH_UTILS_H_

#include <vector>

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace extensions {

class Extension;

// Add launchParams to the JavaScript window object. This will make
// `FileSystemFileHandle` available for file access.
void EnqueueLaunchParamsInWebContents(content::WebContents* web_contents,
                                      const Extension& extension,
                                      const GURL& url,
                                      std::vector<base::FilePath> paths);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FILE_HANDLERS_FILE_HANDLING_LAUNCH_UTILS_H_
