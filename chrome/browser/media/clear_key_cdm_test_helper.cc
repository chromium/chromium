// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/clear_key_cdm_test_helper.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "media/base/media_switches.h"
#include "media/cdm/cdm_paths.h"
#include "media/cdm/clear_key_cdm_common.h"

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
void RegisterClearKeyCdm(base::CommandLine* command_line,
                         bool use_wrong_cdm_path) {
  // External ClearKey is a loadable_module used only in tests.
  base::FilePath cdm_path;
  base::PathService::Get(base::DIR_OUT_TEST_DATA_ROOT, &cdm_path);
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
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(IS_WIN)
void RegisterMediaFoundationClearKeyCdm(
    std::vector<base::test::FeatureRefAndParams>& features) {
  // MediaFoundation ClearKey is a loadable_module used only in tests.
  base::FilePath cdm_path;
  base::PathService::Get(base::DIR_OUT_TEST_DATA_ROOT, &cdm_path);
  cdm_path = cdm_path.AppendASCII(base::GetLoadableModuleName(
      media::kMediaFoundationClearKeyCdmLibraryName));

  features.push_back(base::test::FeatureRefAndParams(
      media::kExternalClearKeyForTesting,
      {{media::kMediaFoundationClearKeyCdmPathForTesting.name,
        cdm_path.MaybeAsASCII()}}));
}
#endif  // BUILDFLAG(IS_WIN)
