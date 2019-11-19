// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_terminal.h"

#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/extensions/api/terminal/terminal_extension_helper.h"
#include "chrome/browser/ui/ash/window_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "net/base/escape.h"

namespace crostini {

GURL GenerateVshInCroshUrl(Profile* profile,
                           const std::string& vm_name,
                           const std::string& container_name,
                           const std::vector<std::string>& terminal_args) {
  std::string vsh_crosh =
      extensions::TerminalExtensionHelper::GetCroshExtensionURL(profile)
          .spec() +
      "?command=vmshell";
  std::string vm_name_param = net::EscapeQueryParamValue(
      base::StringPrintf("--vm_name=%s", vm_name.c_str()), false);
  std::string container_name_param = net::EscapeQueryParamValue(
      base::StringPrintf("--target_container=%s", container_name.c_str()),
      false);
  std::string owner_id_param = net::EscapeQueryParamValue(
      base::StringPrintf("--owner_id=%s",
                         CryptohomeIdForProfile(profile).c_str()),
      false);

  std::vector<std::string> pieces = {vsh_crosh, vm_name_param,
                                     container_name_param, owner_id_param};
  if (!terminal_args.empty()) {
    // Separates the command args from the args we are passing into the
    // terminal to be executed.
    pieces.push_back("--");
    for (auto arg : terminal_args) {
      pieces.push_back(net::EscapeQueryParamValue(arg, false));
    }
  }

  return GURL(base::JoinString(pieces, "&args[]="));
}

apps::AppLaunchParams GenerateTerminalAppLaunchParams() {
  apps::AppLaunchParams launch_params(
      kCrostiniCroshBuiltinAppId,
      apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW,
      apps::mojom::AppLaunchSource::kSourceAppLauncher);
  launch_params.override_app_name =
      AppNameFromCrostiniAppId(kCrostiniTerminalId);
  return launch_params;
}

Browser* CreateContainerTerminal(Profile* profile,
                                 const apps::AppLaunchParams& launch_params,
                                 const GURL& vsh_in_crosh_url) {
  return CreateApplicationWindow(profile, launch_params, vsh_in_crosh_url);
}

void ShowContainerTerminal(Profile* profile,
                           const apps::AppLaunchParams& launch_params,
                           const GURL& vsh_in_crosh_url,
                           Browser* browser) {
  ShowApplicationWindow(profile, launch_params, vsh_in_crosh_url, browser,
                        WindowOpenDisposition::NEW_FOREGROUND_TAB);
  browser->window()->GetNativeWindow()->SetProperty(
      kOverrideWindowIconResourceIdKey, IDR_LOGO_CROSTINI_TERMINAL);
}

void LaunchContainerTerminal(Profile* profile,
                             const std::string& vm_name,
                             const std::string& container_name,
                             const std::vector<std::string>& terminal_args) {
  GURL vsh_in_crosh_url =
      GenerateVshInCroshUrl(profile, vm_name, container_name, terminal_args);
  apps::AppLaunchParams launch_params = GenerateTerminalAppLaunchParams();

  Browser* browser =
      CreateContainerTerminal(profile, launch_params, vsh_in_crosh_url);
  ShowContainerTerminal(profile, launch_params, vsh_in_crosh_url, browser);
}

}  // namespace crostini
