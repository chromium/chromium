// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NATIVE_WINDOW_NOTIFICATION_SOURCE_H_
#define CHROME_BROWSER_NATIVE_WINDOW_NOTIFICATION_SOURCE_H_

#include "build/build_config.h"
#include "content/public/browser/notification_source.h"
#include "ui/gfx/native_widget_types.h"

#if !defined(OS_MACOSX)

namespace content {

// Specialization of the Source class for native windows.  On Windows, these are
// HWNDs rather than pointers, and since the Source class expects a pointer
// type, this is necessary.  On Mac/Linux, these are pointers, so this is
// unnecessary but harmless.
template<>
class Source<gfx::NativeWindow> : public content::NotificationSource {
 public:
  explicit Source(gfx::NativeWindow wnd) : content::NotificationSource(wnd) {}

  explicit Source(const content::NotificationSource& other)
      : content::NotificationSource(other) {}

  gfx::NativeWindow operator->() const { return ptr(); }
  gfx::NativeWindow ptr() const {
    return static_cast<gfx::NativeWindow>(const_cast<void*>(ptr_));
  }
};

}  // namespace content

#endif

#endif  // CHROME_BROWSER_NATIVE_WINDOW_NOTIFICATION_SOURCE_H_
