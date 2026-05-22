// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_GLIC_OS_ICON_PROVIDER_MAC_H_
#define CHROME_BROWSER_BACKGROUND_GLIC_OS_ICON_PROVIDER_MAC_H_

#include <memory>
#include <string>

#include "base/memory/raw_ref.h"
#include "ui/gfx/image/image_skia.h"

class PrefService;

namespace glic {

class GlicStatusIcon;

// Class for selecting an icon for the glic status tray icon on Mac based on the
// presence of the Gemini app's status tray icon. If
// --enable-features=GlicChromeStatusIcon:glic-chrome-status-icon-use-alt-icon/true
// and both Chrome and standalone Gemini are running, set a local pref that
// permanently causes Chrome to show a different icon in the status tray (unless
// reset by disabling that flag).
class OSIconProviderMac {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual bool IsAppRunning(const std::string& bundle_id) const = 0;
  };

  explicit OSIconProviderMac(PrefService& prefs,
                             GlicStatusIcon& glic_status_icon);
  explicit OSIconProviderMac(PrefService& prefs,
                             GlicStatusIcon& glic_status_icon,
                             std::unique_ptr<Delegate> delegate);
  ~OSIconProviderMac();
  gfx::ImageSkia GetIcon() const;

  void OnRunningAppsUpdated(const std::string& bundle_id);

 private:
  void SetUseAltIcon(bool use_alt_icon);
  bool GetUseAltIcon() const;

  std::unique_ptr<Delegate> delegate_;

  // Unowned and guaranteed to outlive this.
  raw_ref<PrefService> prefs_;
  raw_ref<GlicStatusIcon> glic_status_icon_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_BACKGROUND_GLIC_OS_ICON_PROVIDER_MAC_H_
