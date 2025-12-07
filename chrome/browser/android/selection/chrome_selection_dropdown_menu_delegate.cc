// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_selection_dropdown_menu_delegate.h"

#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
#include "chrome/browser/extensions/extension_menu_model_android.h"
#endif  // BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)

namespace android {

ChromeSelectionDropdownMenuDelegate::ChromeSelectionDropdownMenuDelegate() =
    default;

ChromeSelectionDropdownMenuDelegate::~ChromeSelectionDropdownMenuDelegate() =
    default;

// SelectionPopupDelegate implementation.
std::unique_ptr<ui::MenuModel>
ChromeSelectionDropdownMenuDelegate::GetSelectionPopupExtraItems(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
  std::unique_ptr<extensions::ExtensionMenuModel> extension_menu_model =
      std::make_unique<extensions::ExtensionMenuModel>(render_frame_host,
                                                       params);
  extension_menu_model->PopulateModel();
  return std::move(extension_menu_model);
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
}
}  // namespace android
