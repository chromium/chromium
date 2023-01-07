// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_BROWSER_HANDLER_FILE_BROWSER_HANDLER_FLOW_LACROS_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_BROWSER_HANDLER_FILE_BROWSER_HANDLER_FLOW_LACROS_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"

class Profile;

namespace extensions {

class Extension;

// The callback is used for ExecuteFileBrowserHandlerFlow().
using FileBrowserHandlerFlowFinishedCallback =
    base::OnceCallback<void(bool success)>;

// Executes a file browser handler specified by |extension| of the given
// |action_id| for |entry_paths|. Calls |done| on completion.
void ExecuteFileBrowserHandlerFlow(Profile* profile,
                                   const Extension* extension,
                                   const std::string& action_id,
                                   std::vector<base::FilePath>&& entry_paths,
                                   std::vector<std::string>&& mime_types,
                                   FileBrowserHandlerFlowFinishedCallback done);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_BROWSER_HANDLER_FILE_BROWSER_HANDLER_FLOW_LACROS_H_
