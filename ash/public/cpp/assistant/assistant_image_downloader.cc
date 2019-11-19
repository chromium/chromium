// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/assistant_image_downloader.h"

#include "base/logging.h"

namespace ash {
namespace {
AssistantImageDownloader* g_assistant_image_downloader = nullptr;
}

// static
AssistantImageDownloader* AssistantImageDownloader::GetInstance() {
  return g_assistant_image_downloader;
}

AssistantImageDownloader::AssistantImageDownloader() {
  DCHECK(!g_assistant_image_downloader);
  g_assistant_image_downloader = this;
}
AssistantImageDownloader::~AssistantImageDownloader() {
  DCHECK_EQ(g_assistant_image_downloader, this);
  g_assistant_image_downloader = nullptr;
}

}  // namespace ash
