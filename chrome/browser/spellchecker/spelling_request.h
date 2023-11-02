// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLING_REQUEST_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLING_REQUEST_H_

#include "base/containers/unique_ptr_adapters.h"
#include "base/gtest_prod_util.h"
#include "components/spellcheck/browser/platform_spell_checker.h"
#include "components/spellcheck/browser/spell_check_host_impl.h"
#include "components/spellcheck/browser/spelling_service_client.h"

class SpellingRequest;

struct SpellCheckResult;

// SpellingRequest is owned by SpellCheckHostChromeImpl.
class SpellingRequest {
 public:
  using RequestTextCheckCallback =
      spellcheck::mojom::SpellCheckHost::RequestTextCheckCallback;
  using DestructionCallback = base::OnceCallback<void(SpellingRequest*)>;

  // Creates the request, but does not start it. Gives tests finer control over
  // how the request is executed.
  static std::unique_ptr<SpellingRequest> CreateForTest(
      const std::u16string& text,
      RequestTextCheckCallback callback,
      DestructionCallback destruction_callback,
      base::RepeatingClosure completion_barrier);

  SpellingRequest(PlatformSpellChecker* platform_spell_checker,
                  SpellingServiceClient* client,
                  const std::u16string& text,
                  int render_process_id,
                  int document_tag,
                  RequestTextCheckCallback callback,
                  DestructionCallback destruction_callback);

  SpellingRequest(const SpellingRequest&) = delete;
  SpellingRequest& operator=(const SpellingRequest&) = delete;

  ~SpellingRequest();

  // Exposed to tests only.
  static void CombineResults(
      std::vector<SpellCheckResult>* remote_results,
      const std::vector<SpellCheckResult>& local_results);

 private:
  FRIEND_TEST_ALL_PREFIXES(SpellingRequestRemoteCheckUnitTest,
                           OnRemoteCheckCompleted);

  // Test-only constructor that does not kick off the remote and local checks.
  // Use the `CreateForTest` static helper.
  SpellingRequest(const std::u16string& text,
                  RequestTextCheckCallback callback,
                  DestructionCallback destruction_callback,
                  base::RepeatingClosure completion_barrier);

  // Request server-side checking for |text_|.
  void RequestRemoteCheck(SpellingServiceClient* client, int render_process_id);

  // Request a check for |text_| from local spell checker.
  void RequestLocalCheck(PlatformSpellChecker* platform_spell_checker,
                         int document_tag);

  // Check if all pending requests are done, send reply to render process if so.
  void OnCheckCompleted();

  // Called when server-side checking is complete. Must be called on UI thread.
  void OnRemoteCheckCompleted(bool success,
                              const std::u16string& text,
                              const std::vector<SpellCheckResult>& results);

  // Called when local checking is complete. Must be called on UI thread.
  void OnLocalCheckCompleted(const std::vector<SpellCheckResult>& results);

  // Forwards the results back to UI thread when local checking completes.
  static void OnLocalCheckCompletedOnAnyThread(
      base::WeakPtr<SpellingRequest> request,
      const std::vector<SpellCheckResult>& results);

  std::vector<SpellCheckResult> local_results_;
  std::vector<SpellCheckResult> remote_results_;

  // Barrier closure for completion of both remote and local check.
  base::RepeatingClosure completion_barrier_;
  bool remote_success_;

  // The string to be spell-checked.
  std::u16string text_;

  // Callback to send the results to renderer. Note that both RequestTextCheck
  // and RequestPartialTextCheck have the same callback signatures, so both
  // callback types can be assigned to this member (the generated
  // base::OnceCallback types are the same).
  RequestTextCheckCallback callback_;

  // Callback to delete |this|. Called on |this| after everything is done.
  DestructionCallback destruction_callback_;

  base::WeakPtrFactory<SpellingRequest> weak_factory_{this};
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLING_REQUEST_H_
