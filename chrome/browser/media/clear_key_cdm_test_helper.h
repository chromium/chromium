// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CLEAR_KEY_CDM_TEST_HELPER_H_
#define CHROME_BROWSER_MEDIA_CLEAR_KEY_CDM_TEST_HELPER_H_

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "media/media_buildflags.h"

namespace base {
class CommandLine;
}

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
// Registers ClearKeyCdm in |command_line|.
void RegisterClearKeyCdm(base::CommandLine* command_line,
                         bool use_wrong_cdm_path = false);
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(IS_WIN)
// Registers MediaFoundationClearKeyCdm in feature list.
void RegisterMediaFoundationClearKeyCdm(
    std::vector<base::test::FeatureRefAndParams>& features);
#endif  // BUILDFLAG(IS_WIN)

#endif  // CHROME_BROWSER_MEDIA_CLEAR_KEY_CDM_TEST_HELPER_H_
