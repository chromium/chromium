// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_TEXT_FINDER_TEXT_FINDER_MANAGER_H_
#define CHROME_BROWSER_COMPANION_TEXT_FINDER_TEXT_FINDER_MANAGER_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/companion/text_finder/text_finder.h"
#include "content/public/browser/page_user_data.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom.h"

namespace companion {

class TextFinderManager : public content::PageUserData<TextFinderManager> {
 public:
  using AllDoneCallback = base::OnceCallback<void(
      const std::vector<std::pair<std::string, bool>>&)>;

  ~TextFinderManager() override;

  TextFinderManager(const TextFinderManager&) = delete;
  TextFinderManager& operator=(const TextFinderManager&) = delete;

  // Creates a text finder and adds it to manager. Initializes the text finder
  // to perform search through the annotation agent in the renderer process.
  // Calls `callback` with the boolean search result indicating found or not
  // found. Removes the finder from manager upon finishing searching or agent
  // disconnection. Returns the id associated with the created text finder.
  std::optional<base::UnguessableToken> CreateTextFinder(
      const std::string& text_directive,
      TextFinder::FinishedCallback callback);

  // Creates multiple text finders for a vector of text directives. Calls
  // `all_done_callback` when all text finders finish searching (via
  // `base::BarrierCallback`).
  void CreateTextFinders(const std::vector<std::string>& text_directives,
                         AllDoneCallback all_done_callback);

  // Returns the number of text finder instances.
  size_t Size() const;

 private:
  friend class content::PageUserData<TextFinderManager>;
  friend class TextFinderManagerBaseTest;
  FRIEND_TEST_ALL_PREFIXES(TextFinderManagerTest, SingleTextFinderTest);
  FRIEND_TEST_ALL_PREFIXES(TextFinderManagerTest, MultiTextFindersTest);

  explicit TextFinderManager(content::Page& page);

  // Removes the text finder associated with `id`.
  void RemoveTextFinder(base::UnguessableToken id);

  PAGE_USER_DATA_KEY_DECL();

  // Text finders keyed by their unique ids.
  std::unordered_map<base::UnguessableToken,
                     std::unique_ptr<TextFinder>,
                     base::UnguessableTokenHash>
      finder_map_;

  // A connection to the annotation agent container on the renderer side to
  // bind text finder instances to their agent counterpart.
  mojo::Remote<blink::mojom::AnnotationAgentContainer> agent_container_;

  base::WeakPtrFactory<TextFinderManager> weak_ptr_factory_{this};
};

}  // namespace companion

#endif  // CHROME_BROWSER_COMPANION_TEXT_FINDER_TEXT_FINDER_MANAGER_H_
