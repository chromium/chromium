// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spell_check_host_chrome_impl.h"

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "components/spellcheck/browser/spellcheck_host_metrics.h"
#include "components/spellcheck/browser/spellcheck_platform.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
#include "chrome/browser/spellchecker/spelling_request.h"
#endif

namespace {

SpellCheckHostChromeImpl::Binder& GetSpellCheckHostBinderOverride() {
  static base::NoDestructor<SpellCheckHostChromeImpl::Binder> binder;
  return *binder;
}

}  // namespace

SpellCheckHostChromeImpl::SpellCheckHostChromeImpl(int render_process_id)
    : render_process_id_(render_process_id) {}

SpellCheckHostChromeImpl::~SpellCheckHostChromeImpl() = default;

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

void SpellCheckHostChromeImpl::RequestDictionary() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // The renderer has requested that we initialize its spellchecker. This
  // generally should only be called once per session, as after the first
  // call, future renderers will be passed the initialization information
  // on startup (or when the dictionary changes in some way).
  SpellcheckService* spellcheck = GetSpellcheckService();
  if (!spellcheck)
    return;  // Teardown.

  // The spellchecker initialization already started and finished; just
  // send it to the renderer.
  auto* host = content::RenderProcessHost::FromID(render_process_id_);
  if (host)
    spellcheck->InitForRenderer(host);

  // TODO(rlp): Ensure that we do not initialize the hunspell dictionary
  // more than once if we get requests from different renderers.
}

void SpellCheckHostChromeImpl::NotifyChecked(const base::string16& word,
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
    const base::string16& text,
    CallSpellingServiceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (text.empty()) {
    std::move(callback).Run(false, std::vector<SpellCheckResult>());
    mojo::ReportBadMessage(__FUNCTION__);
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
    const base::string16& text,
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

#if defined(OS_MACOSX) || defined(OS_WIN)

void SpellCheckHostChromeImpl::CheckSpelling(const base::string16& word,
                                             int route_id,
                                             CheckSpellingCallback callback) {
  bool correct = spellcheck_platform::CheckSpelling(word, route_id);
  std::move(callback).Run(correct);
}

void SpellCheckHostChromeImpl::FillSuggestionList(
    const base::string16& word,
    FillSuggestionListCallback callback) {
  std::vector<base::string16> suggestions;
  spellcheck_platform::FillSuggestionList(word, &suggestions);
  std::move(callback).Run(suggestions);
}

void SpellCheckHostChromeImpl::RequestTextCheck(
    const base::string16& text,
    int route_id,
    RequestTextCheckCallback callback) {
  DCHECK(!text.empty());
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Initialize the spellcheck service if needed. The service will send the
  // language code for text breaking to the renderer. (Text breaking is required
  // for the context menu to show spelling suggestions.) Initialization must
  // happen on UI thread.
  GetSpellcheckService();

  // |SpellingRequest| self-destructs on completion.
  // OK to store unretained |this| in a |SpellingRequest| owned by |this|.
  requests_.insert(std::make_unique<SpellingRequest>(
      &client_, text, render_process_id_, route_id, std::move(callback),
      base::BindOnce(&SpellCheckHostChromeImpl::OnRequestFinished,
                     base::Unretained(this))));
}

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
#endif  // defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_MACOSX)
int SpellCheckHostChromeImpl::ToDocumentTag(int route_id) {
  if (!tag_map_.count(route_id))
    tag_map_[route_id] = spellcheck_platform::GetDocumentTag();
  return tag_map_[route_id];
}

// TODO(groby): We are currently not notified of retired tags. We need
// to track destruction of RenderViewHosts on the browser process side
// to update our mappings when a document goes away.
void SpellCheckHostChromeImpl::RetireDocumentTag(int route_id) {
  spellcheck_platform::CloseDocumentWithTag(ToDocumentTag(route_id));
  tag_map_.erase(route_id);
}
#endif  // defined(OS_MACOSX)

SpellcheckService* SpellCheckHostChromeImpl::GetSpellcheckService() const {
  auto* host = content::RenderProcessHost::FromID(render_process_id_);
  if (!host)
    return nullptr;
  return SpellcheckServiceFactory::GetForContext(host->GetBrowserContext());
}
