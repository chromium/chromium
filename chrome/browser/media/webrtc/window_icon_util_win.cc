// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/window_icon_util.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/icon_util.h"

gfx::ImageSkia GetWindowIcon(content::DesktopMediaID id) {
  DCHECK(id.type == content::DesktopMediaID::TYPE_WINDOW);

  HWND hwnd = reinterpret_cast<HWND>(id.id);
  HICON icon_handle = 0;

  SendMessageTimeout(hwnd, WM_GETICON, ICON_BIG, 0, SMTO_ABORTIFHUNG, 5,
                     (PDWORD_PTR)&icon_handle);
  if (!icon_handle)
    icon_handle = reinterpret_cast<HICON>(GetClassLongPtr(hwnd, GCLP_HICON));

  if (!icon_handle) {
    SendMessageTimeout(hwnd, WM_GETICON, ICON_SMALL, 0, SMTO_ABORTIFHUNG, 5,
                       (PDWORD_PTR)&icon_handle);
  }
  if (!icon_handle) {
    SendMessageTimeout(hwnd, WM_GETICON, ICON_SMALL2, 0, SMTO_ABORTIFHUNG, 5,
                       (PDWORD_PTR)&icon_handle);
  }
  if (!icon_handle)
    icon_handle = reinterpret_cast<HICON>(GetClassLongPtr(hwnd, GCLP_HICONSM));

  if (!icon_handle)
    return gfx::ImageSkia();

  const SkBitmap icon_bitmap = IconUtil::CreateSkBitmapFromHICON(icon_handle);

  if (icon_bitmap.isNull())
    return gfx::ImageSkia();

  return gfx::ImageSkia::CreateFrom1xBitmap(icon_bitmap);
}
