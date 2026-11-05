// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_TOAST_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_TOAST_H_

#include <memory>

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

// A generalized helper class to show a Glic-branded message toast/banner across
// platforms (via MessageDispatcherBridge on Android, and ToastController on
// Desktop Views). The toast displays Glic's spark icon.
class GlicToast {
 public:
  // Creates and shows a Glic-branded toast message on the given `tab`. Returns
  // the toast instance if successfully shown, or nullptr if the toast cannot be
  // shown (e.g. tab has no contents, tab is in the background, or another toast
  // prevented it).
  static std::unique_ptr<GlicToast> Show(const tabs::TabInterface& tab,
                                         int title_res_id,
                                         int description_res_id = 0);

  virtual ~GlicToast() = default;

  GlicToast(const GlicToast&) = delete;
  GlicToast& operator=(const GlicToast&) = delete;

  virtual bool IsShowing() const = 0;

 protected:
  GlicToast() = default;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_TOAST_H_
