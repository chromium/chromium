// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_TEXT_SESSION_SET_H_
#define CHROME_BROWSER_AI_AI_TEXT_SESSION_SET_H_

#include <variant>

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ai/ai_text_session.h"
#include "content/public/browser/document_user_data.h"

// The data structure that supports adding and removing `AITextSession`.
class AITextSessionSet {
 public:
  // This alias represents the browser-side host of the context that interacts
  // with the `AITextSession`. It can be a `RenderFrameHost` if it's from a
  // document, or a `SupportsUserData` if it's from a worker.
  // When binding the receiver of `blink::mojom::AIManager`, we need to pass the
  // `RenderFrameHost` for document, because we need to wrap the `AITextSession`
  // in a `DocumentUserData` to ensure that it gets properly destroyed when the
  // navigation happens and the RenderFrame is reused (until RenderDocument is
  // launched).
  // We cannot just pass it as `SupportsUserData` because `RenderFrameHost` is
  // not an implementation of `SupportsUserData`.
  using ReceiverContext =
      std::variant<content::RenderFrameHost*, base::SupportsUserData*>;

  AITextSessionSet(const AITextSessionSet&) = delete;
  AITextSessionSet& operator=(const AITextSessionSet&) = delete;
  ~AITextSessionSet();

  // Add an `AITextSession` into the set.
  void AddSession(std::unique_ptr<AITextSession> session);
  // Returns the size of session set for testing purpose.
  size_t GetSessionSetSizeForTesting();

  static AITextSessionSet* GetFromContext(ReceiverContext context);

  // Returns a weak pointer for testing purposes only.
  base::WeakPtr<AITextSessionSet> GetWeakPtrForTesting() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  AITextSessionSet();
  // Remove the `AITextSession` from the set.
  virtual void RemoveSession(AITextSession* session);
  // This is called when all the sessions in the flat_set get removed to clear
  // the `AITextSessionSet` itself.
  virtual void OnAllSessionsRemoved() = 0;

  base::flat_set<std::unique_ptr<AITextSession>, base::UniquePtrComparator>
      sessions_;

 private:
  base::WeakPtrFactory<AITextSessionSet> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_TEXT_SESSION_SET_H_
