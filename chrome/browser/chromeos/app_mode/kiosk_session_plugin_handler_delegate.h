// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_SESSION_PLUGIN_HANDLER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_SESSION_PLUGIN_HANDLER_DELEGATE_H_

#include <set>

#include "base/files/file_path.h"
#include "ppapi/buildflags/buildflags.h"

#if !BUILDFLAG(ENABLE_PLUGINS)
#error "Plugins should be enabled"
#endif

namespace chromeos {

class KioskSessionPluginHandlerDelegate {
 public:
  // Whether the plugin identified by the path should be handled.
  virtual bool ShouldHandlePlugin(const base::FilePath& plugin_path) const = 0;

  // Invoked after a plugin is crashed.
  virtual void OnPluginCrashed(const base::FilePath& plugin_path) = 0;

  // Invoked after plugins are hung.
  virtual void OnPluginHung(const std::set<int>& hung_plugins) = 0;

 protected:
  virtual ~KioskSessionPluginHandlerDelegate() {}
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_SESSION_PLUGIN_HANDLER_DELEGATE_H_
