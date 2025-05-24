// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/passage_embeddings/embedder_tab_observer.h"

#include <numeric>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/history_embeddings/history_embeddings_utils.h"
#include "chrome/browser/passage_embeddings/chrome_passage_embeddings_service_controller.h"
#include "chrome/browser/passage_embeddings/passage_embedder_model_observer_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "components/passage_embeddings/passage_embedder_model_observer.h"
#include "components/passage_embeddings/passage_embeddings_features.h"
#include "components/passage_embeddings/passage_embeddings_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "url/gurl.h"

namespace passage_embeddings {

namespace {

blink::mojom::InnerTextParamsPtr MakeInnerTextParams() {
  auto params = blink::mojom::InnerTextParams::New();
  params->max_words_per_aggregate_passage = kMaxWordsPerAggregatePassage.Get();
  params->max_passages = kMaxPassagesPerPage.Get();
  params->min_words_per_passage = kMinWordsPerPassage.Get();
  return params;
}

void OnGotEmbeddings(base::ElapsedTimer embeddings_computation_timer,
                     std::vector<std::string> passages,
                     std::vector<Embedding> embeddings,
                     Embedder::TaskId task_id,
                     ComputeEmbeddingsStatus status) {
  if (status != ComputeEmbeddingsStatus::kSuccess) {
    return;
  }
  VLOG(3) << "Embeddings computed in "
          << embeddings_computation_timer.Elapsed();
}

}  // namespace

EmbedderTabObserver::EmbedderTabObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

EmbedderTabObserver::~EmbedderTabObserver() = default;

void EmbedderTabObserver::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!PassageEmbedderModelObserverFactory::GetForProfile(GetProfile()) ||
      history_embeddings::IsHistoryEmbeddingsEnabledForProfile(GetProfile())) {
    return;
  }

  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  // Invalidate existing weak pointers to cancel any outstanding delayed tasks.
  weak_ptr_factory_.InvalidateWeakPtrs();
  ScheduleExtraction(render_frame_host->GetWeakDocumentPtr());
}

bool EmbedderTabObserver::ScheduleExtraction(
    content::WeakDocumentPtr weak_render_frame_host) {
  if (!weak_render_frame_host.AsRenderFrameHostIfValid()) {
    VLOG(3) << "Extraction cancelled; no RenderFrameHost at scheduling";
    return false;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&EmbedderTabObserver::MaybeExtractPassages,
                     weak_ptr_factory_.GetWeakPtr(), weak_render_frame_host),
      kPassageExtractionDelay.Get());
  return true;
}

void EmbedderTabObserver::MaybeExtractPassages(
    content::WeakDocumentPtr weak_render_frame_host) {
  // Do not wait for all tabs when using performance scenario.
  // SchedulingEmbedder will use performance scenario which takes loading states
  // into account. By not enforcing this custom non-contention logic, the
  // performance scenario load state handling can be tuned and the feature
  // behavior will follow.
  if (resource_coordinator::TabLoadTracker::Get()->GetLoadingTabCount() > 0 &&
      !kUsePerformanceScenario.Get()) {
    VLOG(3) << "Extraction to be rescheduled; tabs still loading";
    ScheduleExtraction(weak_render_frame_host);
    return;
  }

  ExtractPassages(weak_render_frame_host);
}

void EmbedderTabObserver::ExtractPassages(
    content::WeakDocumentPtr weak_render_frame_host) {
  content::RenderFrameHost* render_frame_host =
      weak_render_frame_host.AsRenderFrameHostIfValid();
  if (!render_frame_host || !render_frame_host->IsRenderFrameLive()) {
    VLOG(3) << "Extraction cancelled; no RenderFrameHost at extraction";
    return;
  }

  base::ElapsedTimer passage_extraction_timer;
  mojo::Remote<blink::mojom::InnerTextAgent> agent;
  render_frame_host->GetRemoteInterfaces()->GetInterface(
      agent.BindNewPipeAndPassReceiver());
  auto* agent_ptr = agent.get();
  agent_ptr->GetInnerText(
      MakeInnerTextParams(),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&EmbedderTabObserver::OnGotPassages,
                         weak_ptr_factory_.GetWeakPtr(), std::move(agent),
                         std::move(passage_extraction_timer)),
          nullptr));
}

void EmbedderTabObserver::OnGotPassages(
    mojo::Remote<blink::mojom::InnerTextAgent> remote,
    base::ElapsedTimer passage_extraction_timer,
    blink::mojom::InnerTextFramePtr mojo_frame) {
  if (!mojo_frame) {
    return;
  }

  std::vector<std::string> passages;
  for (const auto& segment : mojo_frame->segments) {
    if (segment->is_text()) {
      passages.emplace_back(segment->get_text());
    }
  }

  const size_t total_text_size =
      std::accumulate(passages.cbegin(), passages.cend(), 0u,
                      [](size_t acc, const std::string& passage) {
                        return acc + passage.size();
                      });

  VLOG(3) << passages.size() << " passages extracted in "
          << passage_extraction_timer.Elapsed() << " with a total text size of "
          << total_text_size;

  base::ElapsedTimer embeddings_computation_timer;
  ChromePassageEmbeddingsServiceController::Get()
      ->GetEmbedder()
      ->ComputePassagesEmbeddings(
          PassagePriority::kPassive, std::move(passages),
          base::BindOnce(&OnGotEmbeddings,
                         std::move(embeddings_computation_timer)));
}

Profile* EmbedderTabObserver::GetProfile() {
  CHECK(web_contents());
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

}  // namespace passage_embeddings
