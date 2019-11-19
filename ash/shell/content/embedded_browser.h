// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_CONTENT_EMBEDDED_BROWSER_H_
#define ASH_SHELL_CONTENT_EMBEDDED_BROWSER_H_

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "ui/gfx/geometry/rect.h"

class GURL;

namespace aura {
class Window;
}

namespace content {
class BrowserContext;
}

namespace views {
class Widget;
}

namespace ash {
namespace shell {

// Exercises ServerRemoteViewHost to embed a content::WebContents.
class EmbeddedBrowser {
 public:
  aura::Window* GetWindow();

  // Factory.
  static aura::Window* Create(content::BrowserContext* context,
                              const GURL& url,
                              base::Optional<gfx::Rect> bounds = base::nullopt);

 private:
  EmbeddedBrowser(content::BrowserContext* context,
                  const GURL& url,
                  const gfx::Rect& bounds);
  ~EmbeddedBrowser();

  // Callback invoked when the embedding is broken.
  void OnUnembed();

  views::Widget* widget_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedBrowser);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_CONTENT_EMBEDDED_BROWSER_H_
