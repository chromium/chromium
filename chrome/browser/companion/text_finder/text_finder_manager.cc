// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/text_finder/text_finder_manager.h"

#include <memory>

#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/unguessable_token.h"
#include "chrome/browser/companion/text_finder/text_finder.h"
#include "content/public/browser/page.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace companion {

TextFinderManager::TextFinderManager(content::Page& page)
    : PageUserData<TextFinderManager>(page) {
  content::RenderFrameHost& frame = page.GetMainDocument();
  frame.GetRemoteInterfaces()->GetInterface(
      agent_container_.BindNewPipeAndPassReceiver());
}

TextFinderManager::~TextFinderManager() = default;

std::optional<base::UnguessableToken> TextFinderManager::CreateTextFinder(
    const std::string& text_directive,
    TextFinder::FinishedCallback callback) {
  if (text_directive.empty()) {
    std::move(callback).Run(std::make_pair(text_directive, false));
    return std::nullopt;
  }
  // Generate a unique random id.
  const base::UnguessableToken id = base::UnguessableToken::Create();

  finder_map_.emplace(
      id, std::make_unique<TextFinder>(
              text_directive, agent_container_, std::move(callback),
              // Remove this text finder from manager upon agent disconnection.
              base::BindOnce(&TextFinderManager::RemoveTextFinder,
                             weak_ptr_factory_.GetWeakPtr(), id)));
  return id;
}

void TextFinderManager::CreateTextFinders(
    const std::vector<std::string>& text_directives,
    AllDoneCallback all_done_callback) {
  const auto finished_callback =
      base::BarrierCallback<std::pair<std::string, bool>>(
          text_directives.size(), std::move(all_done_callback));
  for (const auto& text_directive : text_directives) {
    CreateTextFinder(text_directive, finished_callback);
  }
}

void TextFinderManager::RemoveTextFinder(base::UnguessableToken id) {
  finder_map_.erase(id);
}

size_t TextFinderManager::Size() const {
  return finder_map_.size();
}

PAGE_USER_DATA_KEY_IMPL(TextFinderManager);

}  // namespace companion
