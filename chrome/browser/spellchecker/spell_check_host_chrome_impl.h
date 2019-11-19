// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELL_CHECK_HOST_CHROME_IMPL_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELL_CHECK_HOST_CHROME_IMPL_H_

#include "base/callback.h"
#include "base/containers/unique_ptr_adapters.h"
#include "build/build_config.h"
#include "components/spellcheck/browser/spell_check_host_impl.h"
#include "components/spellcheck/browser/spelling_service_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

class SpellcheckCustomDictionary;
class SpellcheckService;
class SpellingRequest;

struct SpellCheckResult;

// Implementation of SpellCheckHost involving Chrome-only features.
class SpellCheckHostChromeImpl : public SpellCheckHostImpl {
 public:
  explicit SpellCheckHostChromeImpl(int render_process_id);
  ~SpellCheckHostChromeImpl() override;

  static void Create(
      int render_process_id,
      mojo::PendingReceiver<spellcheck::mojom::SpellCheckHost> receiver);

  // Allows tests to override how |Create()| is implemented to bind a process
  // hosts's SpellCheckHost receiver.
  using Binder = base::RepeatingCallback<void(
      int /* render_process_id */,
      mojo::PendingReceiver<spellcheck::mojom::SpellCheckHost>)>;
  static void OverrideBinderForTesting(Binder binder);

 private:
  friend class TestSpellCheckHostChromeImpl;
  friend class SpellCheckHostChromeImplMacTest;

  // SpellCheckHostImpl:
  void RequestDictionary() override;
  void NotifyChecked(const base::string16& word, bool misspelled) override;

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
  void CallSpellingService(const base::string16& text,
                           CallSpellingServiceCallback callback) override;

  // Invoked when the remote Spelling service has finished checking the
  // text of a CallSpellingService request.
  void CallSpellingServiceDone(
      CallSpellingServiceCallback callback,
      bool success,
      const base::string16& text,
      const std::vector<SpellCheckResult>& service_results) const;

  // Filter out spelling corrections of custom dictionary words from the
  // Spelling service results.
  static std::vector<SpellCheckResult> FilterCustomWordResults(
      const std::string& text,
      const SpellcheckCustomDictionary& custom_dictionary,
      const std::vector<SpellCheckResult>& service_results);
#endif

#if defined(OS_MACOSX) || defined(OS_WIN)
  // Non-Mac and non-Win(i.e., Android) implementations of the following APIs
  // are in the base class SpellCheckHostImpl.
  void CheckSpelling(const base::string16& word,
                     int route_id,
                     CheckSpellingCallback callback) override;
  void FillSuggestionList(const base::string16& word,
                          FillSuggestionListCallback callback) override;
  void RequestTextCheck(const base::string16& text,
                        int route_id,
                        RequestTextCheckCallback callback) override;

  // Clears a finished request from |requests_|. Exposed to SpellingRequest.
  void OnRequestFinished(SpellingRequest* request);

  // Exposed to tests only.
  static void CombineResultsForTesting(
      std::vector<SpellCheckResult>* remote_results,
      const std::vector<SpellCheckResult>& local_results);

  // All pending requests.
  std::set<std::unique_ptr<SpellingRequest>, base::UniquePtrComparator>
      requests_;
#endif  // defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_MACOSX)
  int ToDocumentTag(int route_id);
  void RetireDocumentTag(int route_id);
  std::map<int, int> tag_map_;
#endif  // defined(OS_MACOSX)

  // Returns the SpellcheckService of our |render_process_id_|. The return
  // is null if the render process is being shut down.
  virtual SpellcheckService* GetSpellcheckService() const;

  // The process ID of the renderer.
  const int render_process_id_;

  // A JSON-RPC client that calls the remote Spelling service.
  SpellingServiceClient client_;

  base::WeakPtrFactory<SpellCheckHostChromeImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SpellCheckHostChromeImpl);
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELL_CHECK_HOST_CHROME_IMPL_H_
