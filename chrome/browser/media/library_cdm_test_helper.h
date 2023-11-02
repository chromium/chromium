// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_LIBRARY_CDM_TEST_HELPER_H_
#define CHROME_BROWSER_MEDIA_LIBRARY_CDM_TEST_HELPER_H_

namespace base {
class CommandLine;
}

// Registers ClearKeyCdm in |command_line|.
void RegisterClearKeyCdm(base::CommandLine* command_line,
                         bool use_wrong_cdm_path = false);

#endif  // CHROME_BROWSER_MEDIA_LIBRARY_CDM_TEST_HELPER_H_
