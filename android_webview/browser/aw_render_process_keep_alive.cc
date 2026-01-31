// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_render_process_keep_alive.h"

#include <memory>
#include <utility>

#include "android_webview/common/aw_features.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace android_webview {

namespace {

const void* const kAwRenderProcessKeepAliveKey = &kAwRenderProcessKeepAliveKey;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(RendererKeepAliveEvent)
enum class RendererKeepAliveEvent {
  kReused = 0,
  kTimedOut = 1,
  kPendingReuse = 2,
  kMaxValue = kPendingReuse,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/android/enums.xml:WebViewRendererKeepAliveEvent)

void RecordKeepAliveEvent(RendererKeepAliveEvent event) {
  base::UmaHistogramEnumeration("Android.WebView.RendererKeepAlive.Event",
                                event);
}

}  // namespace

// static
AwRenderProcessKeepAlive*
AwRenderProcessKeepAlive::GetInstanceForRenderProcessHost(
    content::RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  AwRenderProcessKeepAlive* instance = static_cast<AwRenderProcessKeepAlive*>(
      host->GetUserData(kAwRenderProcessKeepAliveKey));
  if (!instance) {
    auto created_instance =
        base::WrapUnique(new AwRenderProcessKeepAlive(host));
    instance = created_instance.get();
    host->SetUserData(kAwRenderProcessKeepAliveKey,
                      std::move(created_instance));
  }
  return instance;
}

AwRenderProcessKeepAlive::AwRenderProcessKeepAlive(
    content::RenderProcessHost* render_process_host)
    : render_process_host_(render_process_host) {}

AwRenderProcessKeepAlive::~AwRenderProcessKeepAlive() = default;

void AwRenderProcessKeepAlive::AddAwContents() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  aw_contents_count_++;
  if (keep_alive_timer_.IsRunning()) {
    RecordKeepAliveEvent(RendererKeepAliveEvent::kReused);
    base::UmaHistogramLongTimes100(
        "Android.WebView.RendererKeepAlive.TimeToReuse",
        base::TimeTicks::Now() - keep_alive_start_time_);
  }
  keep_alive_timer_.Stop();
  if (!kept_alive_) {
    render_process_host_->IncrementPendingReuseRefCount();
    kept_alive_ = true;
  }
}

void AwRenderProcessKeepAlive::RemoveAwContents() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK_GT(aw_contents_count_, 0);
  aw_contents_count_--;
  if (aw_contents_count_ == 0 && kept_alive_) {
    RecordKeepAliveEvent(RendererKeepAliveEvent::kPendingReuse);
    keep_alive_start_time_ = base::TimeTicks::Now();
    keep_alive_timer_.Start(
        FROM_HERE, features::kWebViewRendererKeepAliveDuration.Get(),
        base::BindOnce(&AwRenderProcessKeepAlive::OnKeepAliveTimerFired,
                       weak_factory_.GetWeakPtr()));
  }
}

void AwRenderProcessKeepAlive::OnKeepAliveTimerFired() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(kept_alive_);
  RecordKeepAliveEvent(RendererKeepAliveEvent::kTimedOut);
  render_process_host_->DecrementPendingReuseRefCount();
  kept_alive_ = false;
}

}  // namespace android_webview
