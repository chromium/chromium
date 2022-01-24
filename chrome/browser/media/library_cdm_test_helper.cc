// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/library_cdm_test_helper.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/token.h"
#include "content/public/common/cdm_info.h"
#include "media/base/media_switches.h"
#include "media/cdm/cdm_paths.h"

void RegisterClearKeyCdm(base::CommandLine* command_line,
                         bool use_wrong_cdm_path) {
  // External ClearKey is a loadable_module used only tests.
  base::FilePath cdm_path;
  base::PathService::Get(base::DIR_GEN_TEST_DATA_ROOT, &cdm_path);
  std::string cdm_library_name =
      use_wrong_cdm_path ? "invalidcdmname" : media::kClearKeyCdmLibraryName;
  cdm_path = cdm_path
                 .Append(media::GetPlatformSpecificDirectory(
                     media::kClearKeyCdmBaseDirectory))
                 .AppendASCII(base::GetLoadableModuleName(cdm_library_name));

  // Append the switch to register the Clear Key CDM path.
  command_line->AppendSwitchNative(switches::kClearKeyCdmPathForTesting,
                                   cdm_path.value());
}
