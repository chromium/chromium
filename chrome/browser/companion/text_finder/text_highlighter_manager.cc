// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/text_finder/text_highlighter_manager.h"

#include <memory>

#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/unguessable_token.h"
#include "chrome/browser/companion/text_finder/text_highlighter.h"
#include "content/public/browser/page.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace companion {

using internal::TextHighlighter;

TextHighlighterManager::TextHighlighterManager(content::Page& page)
    : PageUserData<TextHighlighterManager>(page) {
  content::RenderFrameHost& frame = page.GetMainDocument();
  frame.GetRemoteInterfaces()->GetInterface(
      agent_container_.BindNewPipeAndPassReceiver());
}

TextHighlighterManager::~TextHighlighterManager() = default;

void TextHighlighterManager::CreateTextHighlighterAndRemoveExistingInstance(
    const std::string& text_directive) {
  // Remove the existing text highlighter.
  text_highlighter_.reset();

  text_highlighter_ =
      std::make_unique<TextHighlighter>(text_directive, agent_container_);
}

PAGE_USER_DATA_KEY_IMPL(TextHighlighterManager);

}  // namespace companion
