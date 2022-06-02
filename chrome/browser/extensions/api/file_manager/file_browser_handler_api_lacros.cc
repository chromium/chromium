// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_manager/file_browser_handler_api_lacros.h"
#include "base/notreached.h"

FileBrowserHandlerInternalSelectFileFunctionLacros::
    FileBrowserHandlerInternalSelectFileFunctionLacros() = default;

FileBrowserHandlerInternalSelectFileFunctionLacros::
    ~FileBrowserHandlerInternalSelectFileFunctionLacros() = default;

void FileBrowserHandlerInternalSelectFileFunctionLacros::OnFilePathSelected(
    bool success,
    const base::FilePath& full_path) {
  // Called by FileBrowserHandlerInternalSelectFileFunction::Run(), which is
  // stubbed out.
  NOTREACHED();
}

ExtensionFunction::ResponseAction
FileBrowserHandlerInternalSelectFileFunctionLacros::Run() {
  NOTIMPLEMENTED();
  return RespondNow(
      Error("fileBrowserHandlerInternal.selectFile not implemented"));
}

template <>
scoped_refptr<ExtensionFunction>
NewExtensionFunction<FileBrowserHandlerInternalSelectFileFunction>() {
  return base::MakeRefCounted<
      FileBrowserHandlerInternalSelectFileFunctionLacros>();
}
