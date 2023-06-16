// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_TEST_UTIL_H_
#define ASH_TEST_ASH_TEST_UTIL_H_

#include <cstddef>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"

namespace aura {
class Window;
}  // namespace aura

namespace base {
class FilePath;
class TimeDelta;
}  // namespace base

namespace gfx {
class Size;
}  // namespace gfx

namespace views {
class MenuItemView;
}  // namespace views

namespace ash {

// Takes a screenshot of the primary display and saves the screenshot picture to
// the location specified by `file_path`. Returns true if the screenshot is
// taken and saved successfully. Useful for debugging ash unit tests. When using
// this function on an ash unit test, the test code should be executed with
// --enable-pixel-output-in-tests flag.
// NOTE: `file_path` must end with the extension '.png'. If there is an existing
// file matching `file_path`, the existing file will be overwritten.
bool TakePrimaryDisplayScreenshotAndSave(const base::FilePath& file_path);

// Waits for the specified time duration.
// NOTE: this function should only be used for debugging. It should not be used
// in tests or product code.
void GiveItSomeTimeForDebugging(base::TimeDelta time_duration);

// Returns true if the system tray of the root window specified by
// `root_window_index` is visible.
bool IsSystemTrayForRootWindowVisible(size_t root_window_index);

// Creates a pure color image of the specified size.
gfx::ImageSkia CreateSolidColorTestImage(const gfx::Size& image_size,
                                         SkColor color);

// Configures `window` with the specified title and color.
void DecorateWindow(aura::Window* window,
                    const std::u16string& title,
                    SkColor color);

// Waits until there is any visible menu item view with the specified `label`.
// Returns the pointer to the first found target menu item view.
views::MenuItemView* WaitForMenuItemWithLabel(const std::u16string& label);

}  // namespace ash

#endif
