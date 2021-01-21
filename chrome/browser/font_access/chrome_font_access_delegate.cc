// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/font_access/chrome_font_access_delegate.h"

#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/font_access/font_access_chooser.h"
#include "chrome/browser/ui/font_access/font_access_chooser_controller.h"
#include "content/public/browser/render_frame_host.h"

ChromeFontAccessDelegate::ChromeFontAccessDelegate() = default;
ChromeFontAccessDelegate::~ChromeFontAccessDelegate() = default;

std::unique_ptr<content::FontAccessChooser>
ChromeFontAccessDelegate::RunChooser(
    content::RenderFrameHost* frame,
    const std::vector<std::string>& selection,
    content::FontAccessChooser::Callback callback) {
  // TODO(crbug.com/1151464): Decide whether or not to extend/refactor the
  // bubble view launched by chrome::ShowDeviceChooserDialog() or build a new
  // one.
  return std::make_unique<FontAccessChooser>(chrome::ShowDeviceChooserDialog(
      frame, std::make_unique<FontAccessChooserController>(
                 frame, selection, std::move(callback))));
}
