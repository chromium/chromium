// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/media_app/ax_media_app_handler_factory.h"

namespace ash {

// static
AXMediaAppHandlerFactory* AXMediaAppHandlerFactory::GetInstance() {
  static base::NoDestructor<AXMediaAppHandlerFactory> instance;
  return instance.get();
}

AXMediaAppHandlerFactory::AXMediaAppHandlerFactory() = default;
AXMediaAppHandlerFactory::~AXMediaAppHandlerFactory() = default;

std::unique_ptr<AXMediaAppHandler>
AXMediaAppHandlerFactory::CreateAXMediaAppHandler(AXMediaApp* media_app) {
  CHECK(media_app);
  return std::make_unique<AXMediaAppHandler>(media_app);
}

}  // namespace ash
