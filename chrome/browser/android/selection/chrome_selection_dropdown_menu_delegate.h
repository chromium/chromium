// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SELECTION_CHROME_SELECTION_DROPDOWN_MENU_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_SELECTION_CHROME_SELECTION_DROPDOWN_MENU_DELEGATE_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "content/public/browser/android/selection_popup_delegate.h"

namespace content {
struct ContextMenuParams;
class RenderFrameHost;
}  // namespace content

namespace android {

// Delegate to create a menu model containing selected-text context menu items
// from extensions.
class ChromeSelectionDropdownMenuDelegate final
    : public content::SelectionPopupDelegate {
 public:
  ChromeSelectionDropdownMenuDelegate();
  ChromeSelectionDropdownMenuDelegate(
      const ChromeSelectionDropdownMenuDelegate&) = delete;
  ChromeSelectionDropdownMenuDelegate& operator=(
      const ChromeSelectionDropdownMenuDelegate&) = delete;

  ~ChromeSelectionDropdownMenuDelegate() override;

  // SelectionPopupDelegate implementation.
  std::unique_ptr<ui::MenuModel> GetSelectionPopupExtraItems(
      content::RenderFrameHost& render_frame_host,
      const content::ContextMenuParams& params) override;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_SELECTION_CHROME_SELECTION_DROPDOWN_MENU_DELEGATE_H_
