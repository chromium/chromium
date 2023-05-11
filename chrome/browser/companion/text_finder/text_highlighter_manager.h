// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_TEXT_FINDER_TEXT_HIGHLIGHTER_MANAGER_H_
#define CHROME_BROWSER_COMPANION_TEXT_FINDER_TEXT_HIGHLIGHTER_MANAGER_H_

#include <memory>
#include <unordered_map>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "content/public/browser/page_user_data.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom.h"

namespace companion {

namespace internal {
class TextHighlighter;
}

// Manages the creation and deletion of `TextHighlighter` instances.
// There can be at most one instance for the current page at any time, i.e.,
// creating a new one will automatically remove any existing one.
class TextHighlighterManager
    : public content::PageUserData<TextHighlighterManager> {
 public:
  ~TextHighlighterManager() override;

  TextHighlighterManager(const TextHighlighterManager&) = delete;
  TextHighlighterManager& operator=(const TextHighlighterManager&) = delete;

  // Creates a text highlighter and adds it to manager. Removes any existing
  // text hightlighter. Returns the id associated with the created text
  // highlighter.
  void CreateTextHighlighterAndRemoveExistingInstance(
      const std::string& text_directive);

 private:
  friend class content::PageUserData<TextHighlighterManager>;
  FRIEND_TEST_ALL_PREFIXES(TextHighlighterManagerTest, TextHighlighterTest);

  explicit TextHighlighterManager(content::Page& page);

  PAGE_USER_DATA_KEY_DECL();

  // Managed text highlighter instance.
  std::unique_ptr<internal::TextHighlighter> text_highlighter_;

  // A connection to the annotation agent container on the renderer side to
  // bind a text highlighter instance to its agent counterpart.
  mojo::Remote<blink::mojom::AnnotationAgentContainer> agent_container_;

  base::WeakPtrFactory<TextHighlighterManager> weak_ptr_factory_{this};
};

}  // namespace companion

#endif  // CHROME_BROWSER_COMPANION_TEXT_FINDER_TEXT_HIGHLIGHTER_MANAGER_H_
