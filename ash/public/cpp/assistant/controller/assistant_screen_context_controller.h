// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_SCREEN_CONTEXT_CONTROLLER_H_
#define ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_SCREEN_CONTEXT_CONTROLLER_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

class ASH_PUBLIC_EXPORT AssistantScreenContextController {
 public:
  AssistantScreenContextController();
  AssistantScreenContextController(const AssistantScreenContextController&) =
      delete;
  AssistantScreenContextController& operator=(
      const AssistantScreenContextController&) = delete;
  virtual ~AssistantScreenContextController();

  static AssistantScreenContextController* Get();

  // Requests a screenshot of the region enclosed by |rect| and returns the
  // screenshot encoded in JPEG format. If |rect| is empty, it returns a
  // fullscreen screenshot.
  using RequestScreenshotCallback =
      base::OnceCallback<void(const std::vector<uint8_t>&)>;
  virtual void RequestScreenshot(const gfx::Rect& rect,
                                 RequestScreenshotCallback callback) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_SCREEN_CONTEXT_CONTROLLER_H_
