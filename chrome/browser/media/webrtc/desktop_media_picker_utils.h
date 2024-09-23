// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_PICKER_UTILS_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_PICKER_UTILS_H_

#include "base/metrics/histogram_functions.h"
#include "ui/gfx/image/image_skia.h"

// TODO(crbug.com/40267977): Move GDMPreferCurrentTabResult, RecordUma to
// share_this_tab_dialog_views.cc when no longer needed by
// desktop_media_picker_views.cc
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GDMPreferCurrentTabResult {
  kDialogDismissed = 0,                  // Tab/window closed, navigation, etc.
  kUserCancelled = 1,                    // User explicitly cancelled.
  kUserSelectedScreen = 2,               // Screen selected.
  kUserSelectedWindow = 3,               // Window selected.
  kUserSelectedOtherTab = 4,             // Other tab selected from tab-list.
  kUserSelectedThisTabAsGenericTab = 5,  // Current tab selected from tab-list.
  kUserSelectedThisTab = 6,  // Current tab selected from current-tab menu.
  kMaxValue = kUserSelectedThisTab
};

void RecordUma(GDMPreferCurrentTabResult result,
               base::TimeTicks dialog_open_time);

gfx::ImageSkia ScaleBitmap(const SkBitmap& bitmap, gfx::Size size);

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_PICKER_UTILS_H_
