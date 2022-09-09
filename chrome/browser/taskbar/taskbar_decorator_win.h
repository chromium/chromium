// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASKBAR_TASKBAR_DECORATOR_WIN_H_
#define CHROME_BROWSER_TASKBAR_TASKBAR_DECORATOR_WIN_H_

#include <string>

#include "ui/gfx/native_widget_types.h"

class Profile;

namespace gfx {
class Image;
}

namespace taskbar {

// Add a badge with the text |content| to the taskbar.
// |alt_text| will be read by screen readers.
void DrawTaskbarDecorationString(gfx::NativeWindow window,
                                 const std::string& content,
                                 const std::string& alt_text);

// Draws a scaled version of the avatar in |image| on the taskbar button
// associated with top level, visible |window|. Currently only implemented
// for Windows 7 and above.
void DrawTaskbarDecoration(gfx::NativeWindow window, const gfx::Image* image);

// Draws a taskbar icon for non-guest sessions, erases it otherwise. Note: This
// will clear any badge that has been set on the window.
void UpdateTaskbarDecoration(Profile* profile, gfx::NativeWindow window);

}  // namespace taskbar

#endif  // CHROME_BROWSER_TASKBAR_TASKBAR_DECORATOR_WIN_H_
