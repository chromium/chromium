// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/begin_frame_source_webview.h"

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/RootBeginFrameSourceWebView_jni.h"

namespace android_webview {

class BeginFrameSourceWebView::BeginFrameObserver
    : public viz::BeginFrameObserver {
 public:
  BeginFrameObserver(BeginFrameSourceWebView* owner) : owner_(owner) {}

  void OnBeginFrame(const viz::BeginFrameArgs& args) override {
    last_used_begin_frame_args_ = args;
    owner_->SendBeginFrame(args);
  }

  const viz::BeginFrameArgs& LastUsedBeginFrameArgs() const override {
    return last_used_begin_frame_args_;
  }

  void OnBeginFrameSourcePausedChanged(bool paused) override {
    owner_->OnSetBeginFrameSourcePaused(paused);
  }

  bool WantsAnimateOnlyBeginFrames() const override { return true; }

 private:
  const raw_ptr<BeginFrameSourceWebView> owner_;
  viz::BeginFrameArgs last_used_begin_frame_args_;
};

BeginFrameSourceWebView::BeginFrameSourceClient::BeginFrameSourceClient(
    android_webview::BeginFrameSourceWebView* owner)
    : owner_(owner) {}

BeginFrameSourceWebView::BeginFrameSourceClient::~BeginFrameSourceClient() =
    default;
void BeginFrameSourceWebView::BeginFrameSourceClient::OnNeedsBeginFrames(
    bool needs_begin_frames) {
  owner_->OnNeedsBeginFrames(needs_begin_frames);
}

BeginFrameSourceWebView::BeginFrameSourceWebView()
    : ExternalBeginFrameSource(&bfs_client_),
      bfs_client_(this),
      parent_observer_(std::make_unique<BeginFrameObserver>(this)) {
  OnSetBeginFrameSourcePaused(true);
}

BeginFrameSourceWebView::~BeginFrameSourceWebView() {
  if (observed_begin_frame_source_ && !observers_.empty())
    observed_begin_frame_source_->RemoveObserver(parent_observer_.get());
}

void BeginFrameSourceWebView::SetParentSource(BeginFrameSourceWebView* parent) {
  parent_ = parent;
  ObserveBeginFrameSource(parent);
}

void BeginFrameSourceWebView::ObserveBeginFrameSource(
    viz::BeginFrameSource* begin_frame_source) {
  if (observed_begin_frame_source_ == begin_frame_source)
    return;

  if (observed_begin_frame_source_ && !observers_.empty())
    observed_begin_frame_source_->RemoveObserver(parent_observer_.get());

  observed_begin_frame_source_ = begin_frame_source;

  if (observed_begin_frame_source_) {
    if (!observers_.empty())
      observed_begin_frame_source_->AddObserver(parent_observer_.get());
  } else {
    OnSetBeginFrameSourcePaused(true);
  };
}

void BeginFrameSourceWebView::OnNeedsBeginFrames(bool needs_begin_frames) {
  if (needs_begin_frames) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("cc,benchmark", "NeedsBeginFrames", this);
    if (observed_begin_frame_source_)
      observed_begin_frame_source_->AddObserver(parent_observer_.get());
  } else {
    TRACE_EVENT_NESTABLE_ASYNC_END0("cc,benchmark", "NeedsBeginFrames", this);
    if (observed_begin_frame_source_)
      observed_begin_frame_source_->RemoveObserver(parent_observer_.get());
  }
}

void BeginFrameSourceWebView::SendBeginFrame(const viz::BeginFrameArgs& args) {
  DCHECK(!inside_begin_frame_);
  base::AutoReset<bool> inside_bf(&inside_begin_frame_, true);
  OnBeginFrame(args);
  AfterBeginFrame();
}

void BeginFrameSourceWebView::AddBeginFrameCompletionCallback(
    base::OnceClosure callback) {
  DCHECK(parent_);
  DCHECK(inside_begin_frame_);
  parent_->AddBeginFrameCompletionCallback(std::move(callback));
}

const viz::BeginFrameArgs&
BeginFrameSourceWebView::LastDispatchedBeginFrameArgs() {
  return parent_observer_->LastUsedBeginFrameArgs();
}

// static
RootBeginFrameSourceWebView* RootBeginFrameSourceWebView::GetInstance() {
  static base::NoDestructor<RootBeginFrameSourceWebView> instance;
  return instance.get();
}

RootBeginFrameSourceWebView::RootBeginFrameSourceWebView()
    : begin_frame_source_(kNotRestartableId,
                          60.0f,
                          /*requires_align_with_java=*/true),
      j_object_(Java_RootBeginFrameSourceWebView_Constructor(
          jni_zero::AttachCurrentThread(),
          reinterpret_cast<jlong>(this))) {
  ObserveBeginFrameSource(&begin_frame_source_);
}

RootBeginFrameSourceWebView::~RootBeginFrameSourceWebView() = default;

void RootBeginFrameSourceWebView::OnUpdateRefreshRate(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    float refresh_rate) {
  begin_frame_source_.UpdateRefreshRate(refresh_rate);
}

void RootBeginFrameSourceWebView::AfterBeginFrame() {
  // ScopedClosureRunner runs callback in the destructor unless it's cancelled.
  // So this will run all callbacks scheduled to run after BeginFrame completed.
  after_begin_frame_callbacks_.clear();
}

void RootBeginFrameSourceWebView::AddBeginFrameCompletionCallback(
    base::OnceClosure callback) {
  DCHECK(inside_begin_frame());
  after_begin_frame_callbacks_.emplace_back(std::move(callback));
}

}  // namespace android_webview
