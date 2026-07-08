// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_DICTATION_CONTEXT_FETCHER_H_
#define CHROME_BROWSER_DICTATION_DICTATION_CONTEXT_FETCHER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/dictation/dictation_context.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"

namespace dictation {

class Target;

class DictationContextFetcher {
 public:
  DictationContextFetcher();
  ~DictationContextFetcher();

  DictationContextFetcher(const DictationContextFetcher&) = delete;
  DictationContextFetcher& operator=(const DictationContextFetcher&) = delete;

  using GetContextCallback = base::OnceCallback<void(DictationContext)>;
  void Fetch(const Target& target, GetContextCallback callback);

 private:
  void OnPageContextFetched(
      GetContextCallback callback,
      page_content_annotations::FetchPageContextResultCallbackArg result);

  base::WeakPtrFactory<DictationContextFetcher> weak_ptr_factory_{this};
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_DICTATION_CONTEXT_FETCHER_H_
