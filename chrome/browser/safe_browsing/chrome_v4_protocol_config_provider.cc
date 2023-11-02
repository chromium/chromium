// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_v4_protocol_config_provider.h"

#include <string>

#include "base/command_line.h"
#include "build/branding_buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/install_static/install_util.h"
#endif

namespace safe_browsing {

std::string GetProtocolConfigClientName() {
  std::string client_name;
  // On Windows, get the safe browsing client name from the browser
  // distribution classes in installer util. These classes don't yet have
  // an analog on non-Windows builds so just keep the name specified here.
#if BUILDFLAG(IS_WIN)
  client_name = install_static::GetSafeBrowsingName();
#else
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  client_name = "googlechrome";
#else
  client_name = "chromium";
#endif

  // Mark client string to allow server to differentiate mobile.
#if BUILDFLAG(IS_ANDROID)
  client_name.append("-a");
#endif

#endif  // BUILDFLAG(IS_WIN)

  return client_name;
}

V4ProtocolConfig GetV4ProtocolConfig() {
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  return ::safe_browsing::GetV4ProtocolConfig(
      GetProtocolConfigClientName(),
      cmdline->HasSwitch(::switches::kDisableBackgroundNetworking));
}

}  // namespace safe_browsing
