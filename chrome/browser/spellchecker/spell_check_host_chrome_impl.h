// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELL_CHECK_HOST_CHROME_IMPL_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELL_CHECK_HOST_CHROME_IMPL_H_

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/spellcheck/browser/spell_check_host_impl.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

#if BUILDFLAG(ENABLE_SPELLING_SERVICE)
#include "components/spellcheck/browser/spelling_service_client.h"
#endif

class SpellcheckCustomDictionary;
class SpellcheckService;
class SpellingRequest;

struct SpellCheckResult;

// Implementation of SpellCheckHost involving Chrome-only features.
class SpellCheckHostChromeImpl : public SpellCheckHostImpl {
 public:
  explicit SpellCheckHostChromeImpl(int render_process_id);

  SpellCheckHostChromeImpl(const SpellCheckHostChromeImpl&) = delete;
  SpellCheckHostChromeImpl& operator=(const SpellCheckHostChromeImpl&) = delete;

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
  void NotifyChecked(const std::u16string& word, bool misspelled) override;

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
  void CallSpellingService(const std::u16string& text,
                           CallSpellingServiceCallback callback) override;

  // Invoked when the remote Spelling service has finished checking the
  // text of a CallSpellingService request.
  void CallSpellingServiceDone(
      CallSpellingServiceCallback callback,
      bool success,
      const std::u16string& text,
      const std::vector<SpellCheckResult>& service_results) const;

  // Filter out spelling corrections of custom dictionary words from the
  // Spelling service results.
  static std::vector<SpellCheckResult> FilterCustomWordResults(
      const std::string& text,
      const SpellcheckCustomDictionary& custom_dictionary,
      const std::vector<SpellCheckResult>& service_results);
#endif

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER) && BUILDFLAG(ENABLE_SPELLING_SERVICE)
  // Implementations of the following APIs for build configs that don't use the
  // spelling service are in the base class SpellCheckHostImpl.
  void CheckSpelling(const std::u16string& word,
                     CheckSpellingCallback callback) override;
  void FillSuggestionList(const std::u16string& word,
                          FillSuggestionListCallback callback) override;
  void RequestTextCheck(const std::u16string& text,
                        RequestTextCheckCallback callback) override;

#if BUILDFLAG(IS_WIN)
  void InitializeDictionaries(InitializeDictionariesCallback callback) override;
#endif  // BUILDFLAG(IS_WIN)

  // Clears a finished request from |requests_|. Exposed to SpellingRequest.
  void OnRequestFinished(SpellingRequest* request);

  // Exposed to tests only.
  static void CombineResultsForTesting(
      std::vector<SpellCheckResult>* remote_results,
      const std::vector<SpellCheckResult>& local_results);

#if BUILDFLAG(IS_WIN)
  void OnDictionariesInitialized();

  // Callback passed as argument to InitializeDictionaries, and invoked when
  // the dictionaries are loaded for the first time.
  InitializeDictionariesCallback dictionaries_loaded_callback_;
#endif  // BUILDFLAG(IS_WIN)

  // All pending requests.
  std::set<std::unique_ptr<SpellingRequest>, base::UniquePtrComparator>
      requests_;
#endif  //  BUILDFLAG(USE_BROWSER_SPELLCHECKER) &&
        //  BUILDFLAG(ENABLE_SPELLING_SERVICE)

  // Returns the SpellcheckService of our |render_process_id_|. The return
  // is null if the render process is being shut down.
  virtual SpellcheckService* GetSpellcheckService() const;

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  const int document_tag_;
#endif

  // The process ID of the renderer.
  const int render_process_id_;

#if BUILDFLAG(ENABLE_SPELLING_SERVICE)
  // A JSON-RPC client that calls the remote Spelling service.
  SpellingServiceClient client_;
#endif

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
  base::WeakPtrFactory<SpellCheckHostChromeImpl> weak_factory_{this};
#endif
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELL_CHECK_HOST_CHROME_IMPL_H_
