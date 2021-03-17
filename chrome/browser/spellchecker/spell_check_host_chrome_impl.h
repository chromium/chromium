// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELL_CHECK_HOST_CHROME_IMPL_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELL_CHECK_HOST_CHROME_IMPL_H_

#include "base/callback.h"
#include "base/containers/unique_ptr_adapters.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/spellcheck/browser/spell_check_host_impl.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

#if BUILDFLAG(ENABLE_SPELLING_SERVICE)
#include "components/spellcheck/browser/spelling_service_client.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/chromeos/input_method/grammar_service_client.h"
#endif

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
                     int route_id,
                     CheckSpellingCallback callback) override;
  void FillSuggestionList(const std::u16string& word,
                          FillSuggestionListCallback callback) override;
  void RequestTextCheck(const std::u16string& text,
                        int route_id,
                        RequestTextCheckCallback callback) override;

#if defined(OS_WIN)
  void InitializeDictionaries(InitializeDictionariesCallback callback) override;
#endif  // defined(OS_WIN)

  // Clears a finished request from |requests_|. Exposed to SpellingRequest.
  void OnRequestFinished(SpellingRequest* request);

  // Exposed to tests only.
  static void CombineResultsForTesting(
      std::vector<SpellCheckResult>* remote_results,
      const std::vector<SpellCheckResult>& local_results);

#if defined(OS_WIN)
  void OnDictionariesInitialized();

  // Callback passed as argument to InitializeDictionaries, and invoked when
  // the dictionaries are loaded for the first time.
  InitializeDictionariesCallback dictionaries_loaded_callback_;
#endif  // defined(OS_WIN)

  // All pending requests.
  std::set<std::unique_ptr<SpellingRequest>, base::UniquePtrComparator>
      requests_;
#endif  //  BUILDFLAG(USE_BROWSER_SPELLCHECKER) &&
        //  BUILDFLAG(ENABLE_SPELLING_SERVICE)

#if defined(OS_MAC)
  int ToDocumentTag(int route_id);
  void RetireDocumentTag(int route_id);
  std::map<int, int> tag_map_;
#endif  // defined(OS_MAC)

  // Returns the SpellcheckService of our |render_process_id_|. The return
  // is null if the render process is being shut down.
  virtual SpellcheckService* GetSpellcheckService() const;

  // The process ID of the renderer.
  const int render_process_id_;

#if BUILDFLAG(ENABLE_SPELLING_SERVICE)
  // A JSON-RPC client that calls the remote Spelling service.
  SpellingServiceClient client_;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Invoked when the on-device grammar service has finished checking the
  // text of a CallSpellingService request.
  void CallGrammarServiceDone(
      CallSpellingServiceCallback callback,
      bool success,
      const std::vector<SpellCheckResult>& service_results) const;

  chromeos::GrammarServiceClient grammar_client_;
#endif

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
  base::WeakPtrFactory<SpellCheckHostChromeImpl> weak_factory_{this};
#endif

  DISALLOW_COPY_AND_ASSIGN(SpellCheckHostChromeImpl);
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELL_CHECK_HOST_CHROME_IMPL_H_
