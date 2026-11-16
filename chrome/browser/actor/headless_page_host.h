// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_HEADLESS_PAGE_HOST_H_
#define CHROME_BROWSER_ACTOR_HEADLESS_PAGE_HOST_H_

#include <memory>

#include "base/functional/callback_helpers.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class Size;
}  // namespace gfx

namespace views {
class Widget;
}  // namespace views

namespace actor {

// Keeps a headless page rendering as if it were visible, so it can be
// captured. On Aura a page only draws frames while its view is in a window
// tree, so the page is hosted in a hidden widget. Elsewhere this does nothing.
// Must be destroyed before its page.
class HeadlessPageHost {
 public:
  HeadlessPageHost(content::WebContents* contents, const gfx::Size& size);
  HeadlessPageHost(const HeadlessPageHost&) = delete;
  HeadlessPageHost& operator=(const HeadlessPageHost&) = delete;
  ~HeadlessPageHost();

 private:
#if defined(USE_AURA)
  // The widget that gives the page a window tree. It is never shown.
  std::unique_ptr<views::Widget> widget_;
  // Keeps the page rendering as if visible, though its widget is hidden.
  base::ScopedClosureRunner capturer_;
#endif
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_HEADLESS_PAGE_HOST_H_
