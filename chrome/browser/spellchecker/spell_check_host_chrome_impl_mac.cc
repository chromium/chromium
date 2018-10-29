// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spell_check_host_chrome_impl.h"

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/task/post_task.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "components/spellcheck/browser/spellcheck_platform.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "services/service_manager/public/cpp/identity.h"

using content::BrowserThread;
using content::BrowserContext;

namespace {

bool CompareLocation(const SpellCheckResult& r1, const SpellCheckResult& r2) {
  return r1.location < r2.location;
}

// Adjusts remote_results by examining local_results. Any result that's both
// local and remote stays type SPELLING, all others are flagged GRAMMAR.
// (This is needed to force gray underline for remote-only results.)
void CombineResults(std::vector<SpellCheckResult>* remote_results,
                    const std::vector<SpellCheckResult>& local_results) {
  std::vector<SpellCheckResult>::const_iterator local_iter(
      local_results.begin());
  std::vector<SpellCheckResult>::iterator remote_iter;

  for (remote_iter = remote_results->begin();
       remote_iter != remote_results->end(); ++remote_iter) {
    // Discard all local results occurring before remote result.
    while (local_iter != local_results.end() &&
           local_iter->location < remote_iter->location) {
      local_iter++;
    }

    // Unless local and remote result coincide, result is GRAMMAR.
    remote_iter->decoration = SpellCheckResult::GRAMMAR;
    if (local_iter != local_results.end() &&
        local_iter->location == remote_iter->location &&
        local_iter->length == remote_iter->length) {
      remote_iter->decoration = SpellCheckResult::SPELLING;
    }
  }
}

}  // namespace

// SpellingRequest is owned by SpellCheckHostChromeImpl.
class SpellingRequest {
 public:
  using RequestTextCheckCallback =
      spellcheck::mojom::SpellCheckHost::RequestTextCheckCallback;
  using DestructionCallback = base::OnceCallback<void(SpellingRequest*)>;

  SpellingRequest(SpellingServiceClient* client,
                  const base::string16& text,
                  const service_manager::Identity& renderer_identity,
                  int document_tag,
                  RequestTextCheckCallback callback,
                  DestructionCallback destruction_callback);

 private:
  // Request server-side checking for |text_|.
  void RequestRemoteCheck(SpellingServiceClient* client,
                          const service_manager::Identity& renderer_identity);

  // Request a check for |text_| from local spell checker.
  void RequestLocalCheck(int document_tag);

  // Check if all pending requests are done, send reply to render process if so.
  void OnCheckCompleted();

  // Called when server-side checking is complete. Must be called on UI thread.
  void OnRemoteCheckCompleted(bool success,
                              const base::string16& text,
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
  base::string16 text_;

  // Callback to send the results to renderer.
  RequestTextCheckCallback callback_;

  // Callback to delete |this|. Called on |this| after everything is done.
  DestructionCallback destruction_callback_;

  base::WeakPtrFactory<SpellingRequest> weak_factory_;
};

SpellingRequest::SpellingRequest(
    SpellingServiceClient* client,
    const base::string16& text,
    const service_manager::Identity& renderer_identity,
    int document_tag,
    RequestTextCheckCallback callback,
    DestructionCallback destruction_callback)
    : remote_success_(false),
      text_(text),
      callback_(std::move(callback)),
      destruction_callback_(std::move(destruction_callback)),
      weak_factory_(this) {
  DCHECK(!text_.empty());
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  completion_barrier_ =
      BarrierClosure(2, base::BindOnce(&SpellingRequest::OnCheckCompleted,
                                       weak_factory_.GetWeakPtr()));
  RequestRemoteCheck(client, renderer_identity);
  RequestLocalCheck(document_tag);
}

void SpellingRequest::RequestRemoteCheck(
    SpellingServiceClient* client,
    const service_manager::Identity& renderer_identity) {
  BrowserContext* context =
      content::BrowserContext::GetBrowserContextForServiceUserId(
          renderer_identity.user_id());

  // |this| may be gone at callback invocation if the owner has been removed.
  client->RequestTextCheck(
      context, SpellingServiceClient::SPELLCHECK, text_,
      base::BindOnce(&SpellingRequest::OnRemoteCheckCompleted,
                     weak_factory_.GetWeakPtr()));
}

void SpellingRequest::RequestLocalCheck(int document_tag) {
  // |this| may be gone at callback invocation if the owner has been removed.
  spellcheck_platform::RequestTextCheck(
      document_tag, text_,
      base::BindOnce(&SpellingRequest::OnLocalCheckCompletedOnAnyThread,
                     weak_factory_.GetWeakPtr()));
}

void SpellingRequest::OnCheckCompleted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<SpellCheckResult>* check_results = &local_results_;
  if (remote_success_) {
    std::sort(remote_results_.begin(), remote_results_.end(), CompareLocation);
    std::sort(local_results_.begin(), local_results_.end(), CompareLocation);
    CombineResults(&remote_results_, local_results_);
    check_results = &remote_results_;
  }

  std::move(callback_).Run(*check_results);

  std::move(destruction_callback_).Run(this);

  // |destruction_callback_| removes |this|. No more operations allowed.
}

void SpellingRequest::OnRemoteCheckCompleted(
    bool success,
    const base::string16& text,
    const std::vector<SpellCheckResult>& results) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  remote_success_ = success;
  remote_results_ = results;
  completion_barrier_.Run();
}

// static
void SpellingRequest::OnLocalCheckCompletedOnAnyThread(
    base::WeakPtr<SpellingRequest> request,
    const std::vector<SpellCheckResult>& results) {
  // Local checking can happen on any thread - don't DCHECK thread.
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&SpellingRequest::OnLocalCheckCompleted, request,
                     results));
}

void SpellingRequest::OnLocalCheckCompleted(
    const std::vector<SpellCheckResult>& results) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  local_results_ = results;
  completion_barrier_.Run();
}

SpellCheckHostChromeImpl::SpellCheckHostChromeImpl(
    const service_manager::Identity& renderer_identity)
    : renderer_identity_(renderer_identity), weak_factory_(this) {}

SpellCheckHostChromeImpl::~SpellCheckHostChromeImpl() = default;

// static
void SpellCheckHostChromeImpl::CombineResultsForTesting(
    std::vector<SpellCheckResult>* remote_results,
    const std::vector<SpellCheckResult>& local_results) {
  CombineResults(remote_results, local_results);
}

void SpellCheckHostChromeImpl::CheckSpelling(const base::string16& word,
                                             int route_id,
                                             CheckSpellingCallback callback) {
  bool correct =
      spellcheck_platform::CheckSpelling(word, ToDocumentTag(route_id));
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
      &client_, text, renderer_identity_, ToDocumentTag(route_id),
      std::move(callback),
      base::BindOnce(&SpellCheckHostChromeImpl::OnRequestFinished,
                     base::Unretained(this))));
}

void SpellCheckHostChromeImpl::OnRequestFinished(SpellingRequest* request) {
  auto iterator = requests_.find(request);
  requests_.erase(iterator);
}

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
