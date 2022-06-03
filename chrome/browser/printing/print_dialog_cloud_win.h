// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_DIALOG_CLOUD_WIN_H_
#define CHROME_BROWSER_PRINTING_PRINT_DIALOG_CLOUD_WIN_H_

class Profile;

namespace base {
class CommandLine;
}

namespace print_dialog_cloud {

// Parse switches from command_line and display the print dialog as appropriate.
bool CreatePrintDialogFromCommandLine(Profile* profile,
                                      const base::CommandLine& command_line);

}  // namespace print_dialog_cloud

#endif  // CHROME_BROWSER_PRINTING_PRINT_DIALOG_CLOUD_WIN_H_
