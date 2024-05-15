// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spell_check_host_chrome_impl.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "components/spellcheck/browser/spellcheck_host_metrics.h"
#include "components/spellcheck/browser/spellcheck_platform.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER) && BUILDFLAG(ENABLE_SPELLING_SERVICE)
#include "chrome/browser/spellchecker/spelling_request.h"
#endif
namespace {

SpellCheckHostChromeImpl::Binder& GetSpellCheckHostBinderOverride() {
  static base::NoDestructor<SpellCheckHostChromeImpl::Binder> binder;
  return *binder;
}

}  // namespace

SpellCheckHostChromeImpl::SpellCheckHostChromeImpl(int render_process_id)
    :
#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
      document_tag_(spellcheck_platform::GetDocumentTag()),
#endif
      render_process_id_(render_process_id) {
}

SpellCheckHostChromeImpl::~SpellCheckHostChromeImpl() {
#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  spellcheck_platform::CloseDocumentWithTag(document_tag_);
#endif
}

// static
void SpellCheckHostChromeImpl::Create(
    int render_process_id,
    mojo::PendingReceiver<spellcheck::mojom::SpellCheckHost> receiver) {
  auto& binder = GetSpellCheckHostBinderOverride();
  if (binder) {
    binder.Run(render_process_id, std::move(receiver));
    return;
  }

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<SpellCheckHostChromeImpl>(render_process_id),
      std::move(receiver));
}

// static
void SpellCheckHostChromeImpl::OverrideBinderForTesting(Binder binder) {
  GetSpellCheckHostBinderOverride() = std::move(binder);
}

void SpellCheckHostChromeImpl::NotifyChecked(const std::u16string& word,
                                             bool misspelled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  SpellcheckService* spellcheck = GetSpellcheckService();
  if (!spellcheck)
    return;  // Teardown.
  if (spellcheck->GetMetrics())
    spellcheck->GetMetrics()->RecordCheckedWordStats(word, misspelled);
}

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
void SpellCheckHostChromeImpl::CallSpellingService(
    const std::u16string& text,
    CallSpellingServiceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (text.empty()) {
    std::move(callback).Run(false, std::vector<SpellCheckResult>());
    mojo::ReportBadMessage("Requested spelling service with empty text");
    return;
  }

  // Checks the user profile and sends a JSON-RPC request to the Spelling
  // service if a user enables the "Use enhanced spell check" option. When
  // a response is received (including an error) from the remote Spelling
  // service, calls CallSpellingServiceDone.
  auto* host = content::RenderProcessHost::FromID(render_process_id_);
  if (!host) {
    std::move(callback).Run(false, std::vector<SpellCheckResult>());
    return;
  }
  client_.RequestTextCheck(
      host->GetBrowserContext(), SpellingServiceClient::SPELLCHECK, text,
      base::BindOnce(&SpellCheckHostChromeImpl::CallSpellingServiceDone,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void SpellCheckHostChromeImpl::CallSpellingServiceDone(
    CallSpellingServiceCallback callback,
    bool success,
    const std::u16string& text,
    const std::vector<SpellCheckResult>& service_results) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  SpellcheckService* spellcheck = GetSpellcheckService();
  if (!spellcheck) {  // Teardown.
    std::move(callback).Run(false, std::vector<SpellCheckResult>());
    return;
  }

  std::vector<SpellCheckResult> results = FilterCustomWordResults(
      base::UTF16ToUTF8(text), *spellcheck->GetCustomDictionary(),
      service_results);

  std::move(callback).Run(success, results);
}

// static
std::vector<SpellCheckResult> SpellCheckHostChromeImpl::FilterCustomWordResults(
    const std::string& text,
    const SpellcheckCustomDictionary& custom_dictionary,
    const std::vector<SpellCheckResult>& service_results) {
  std::vector<SpellCheckResult> results;
  for (const auto& result : service_results) {
    const std::string word = text.substr(result.location, result.length);
    if (!custom_dictionary.HasWord(word))
      results.push_back(result);
  }

  return results;
}
#endif  // BUILDFLAG(USE_RENDERER_SPELLCHECKER)

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER) && BUILDFLAG(ENABLE_SPELLING_SERVICE)
void SpellCheckHostChromeImpl::CheckSpelling(const std::u16string& word,
                                             CheckSpellingCallback callback) {
  bool correct = spellcheck_platform::CheckSpelling(word, document_tag_);
  std::move(callback).Run(correct);
}

void SpellCheckHostChromeImpl::FillSuggestionList(
    const std::u16string& word,
    FillSuggestionListCallback callback) {
  std::vector<std::u16string> suggestions;
  spellcheck_platform::FillSuggestionList(word, &suggestions);
  std::move(callback).Run(suggestions);
}

void SpellCheckHostChromeImpl::RequestTextCheck(
    const std::u16string& text,
    RequestTextCheckCallback callback) {
  DCHECK(!text.empty());
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Initialize the spellcheck service if needed. The service will send the
  // language code for text breaking to the renderer. (Text breaking is required
  // for the context menu to show spelling suggestions.) Initialization must
  // happen on UI thread.
  SpellcheckService* spellcheck = GetSpellcheckService();

  if (!spellcheck) {  // Teardown.
    std::move(callback).Run({});
    return;
  }

  // OK to store unretained |this| in a |SpellingRequest| owned by |this|.
  requests_.insert(std::make_unique<SpellingRequest>(
      spellcheck->platform_spell_checker(), &client_, text, render_process_id_,
      document_tag_, std::move(callback),
      base::BindOnce(&SpellCheckHostChromeImpl::OnRequestFinished,
                     base::Unretained(this))));
}

#if BUILDFLAG(IS_WIN)
void SpellCheckHostChromeImpl::InitializeDictionaries(
    InitializeDictionariesCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (base::FeatureList::IsEnabled(
          spellcheck::kWinDelaySpellcheckServiceInit)) {
    // Initialize the spellcheck service if needed. Initialization must
    // happen on UI thread.
    SpellcheckService* spellcheck = GetSpellcheckService();

    if (!spellcheck) {  // Teardown.
      std::move(callback).Run(/*dictionaries=*/{}, /*custom_words=*/{},
                              /*enable=*/false);
      return;
    }

    dictionaries_loaded_callback_ = std::move(callback);

    spellcheck->InitializeDictionaries(
        base::BindOnce(&SpellCheckHostChromeImpl::OnDictionariesInitialized,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  NOTREACHED_IN_MIGRATION();
  std::move(callback).Run(/*dictionaries=*/{}, /*custom_words=*/{},
                          /*enable=*/false);
}

void SpellCheckHostChromeImpl::OnDictionariesInitialized() {
  DCHECK(dictionaries_loaded_callback_);
  SpellcheckService* spellcheck = GetSpellcheckService();

  if (!spellcheck) {  // Teardown.
    std::move(dictionaries_loaded_callback_)
        .Run(/*dictionaries=*/{}, /*custom_words=*/{},
             /*enable=*/false);
    return;
  }

  const bool enable = spellcheck->IsSpellcheckEnabled();

  std::vector<spellcheck::mojom::SpellCheckBDictLanguagePtr> dictionaries;
  std::vector<std::string> custom_words;
  if (enable) {
    for (const auto& hunspell_dictionary :
         spellcheck->GetHunspellDictionaries()) {
      dictionaries.push_back(spellcheck::mojom::SpellCheckBDictLanguage::New(
          hunspell_dictionary->GetDictionaryFile().Duplicate(),
          hunspell_dictionary->GetLanguage()));
    }

    SpellcheckCustomDictionary* custom_dictionary =
        spellcheck->GetCustomDictionary();
    custom_words.assign(custom_dictionary->GetWords().begin(),
                        custom_dictionary->GetWords().end());
  }

  std::move(dictionaries_loaded_callback_)
      .Run(std::move(dictionaries), custom_words, enable);
}
#endif  // BUILDFLAG(IS_WIN)

void SpellCheckHostChromeImpl::OnRequestFinished(SpellingRequest* request) {
  auto iterator = requests_.find(request);
  requests_.erase(iterator);
}

// static
void SpellCheckHostChromeImpl::CombineResultsForTesting(
    std::vector<SpellCheckResult>* remote_results,
    const std::vector<SpellCheckResult>& local_results) {
  SpellingRequest::CombineResults(remote_results, local_results);
}
#endif  //  BUILDFLAG(USE_BROWSER_SPELLCHECKER) &&
        //  BUILDFLAG(ENABLE_SPELLING_SERVICE)

SpellcheckService* SpellCheckHostChromeImpl::GetSpellcheckService() const {
  auto* host = content::RenderProcessHost::FromID(render_process_id_);
  if (!host)
    return nullptr;
  return SpellcheckServiceFactory::GetForContext(host->GetBrowserContext());
}
