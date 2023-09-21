// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_HANDLER_FACTORY_H_
#define CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_HANDLER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/no_destructor.h"
#include "chrome/browser/accessibility/media_app/ax_media_app.h"
#include "chrome/browser/accessibility/media_app/ax_media_app_handler.h"

namespace ash {

// Factory to create an instance of `AXMediaAppHandler` used by the Media App
// (AKA Backlight) to communicate with the accessibility layer.
class AXMediaAppHandlerFactory final {
 public:
  static AXMediaAppHandlerFactory* GetInstance();

  AXMediaAppHandlerFactory(const AXMediaAppHandlerFactory&) = delete;
  AXMediaAppHandlerFactory& operator=(const AXMediaAppHandlerFactory&) = delete;
  ~AXMediaAppHandlerFactory();

  std::unique_ptr<AXMediaAppHandler> CreateAXMediaAppHandler(
      AXMediaApp* media_app);

 private:
  friend base::NoDestructor<AXMediaAppHandlerFactory>;

  AXMediaAppHandlerFactory();
};

}  // namespace ash

#endif  // CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_HANDLER_FACTORY_H_
