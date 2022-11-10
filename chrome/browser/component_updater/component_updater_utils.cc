// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_updater_utils.h"

#include "build/build_config.h"
#if BUILDFLAG(IS_WIN)
#include "chrome/installer/util/install_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace component_updater {

bool IsPerUserInstall() {
#if BUILDFLAG(IS_WIN)
  // The installer computes and caches this value in memory during the
  // process start up.
  return InstallUtil::IsPerUserInstall();
#else
  return true;
#endif
}

}  // namespace component_updater
