// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_MEMORY_BANK_CHAT_TYPES_H_
#define CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_MEMORY_BANK_CHAT_TYPES_H_

#include <optional>
#include <string>
#include <vector>

#include "components/optimization_guide/proto/features/context_hub.pb.h"
#include "url/gurl.h"

namespace context_hub {

// Represents a cited memory bank entry in a chat response.
struct MemoryBankChatCitation {
  std::string title;
  // The URL of the page where the cited entry originated.
  GURL url;
  // The source URL of the image when the cited entry is of type
  // `MemoryBankType::kImage` (may point to third-party web content).
  std::optional<GURL> image_url;
};

// Represents a single turn in the memory bank chat history, pairing the
// proto turn sent to model execution with any source citations resolved for
// display in the WebUI.
struct MemoryBankChatHistoryTurn {
  optimization_guide::proto::ChatHistoryTurn turn;
  std::vector<MemoryBankChatCitation> citations;
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_MEMORY_BANK_CHAT_TYPES_H_
