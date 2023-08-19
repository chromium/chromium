// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_BEGIN_FRAME_SOURCE_WEBVIEW_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_BEGIN_FRAME_SOURCE_WEBVIEW_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/frame_sinks/external_begin_frame_source_android.h"

namespace android_webview {

// The BeginFrameSourceWebView implements ExternalBeginFrameSource by observing
// another begin_frame_source and provides AfterBeginFrame callback that called
// after BeginFrame is sent out to all observers. It supports hierarchy
// BeginFrameSourceWebView to provide AddBeginFrameCompletionCallback which will
// be forwarded to root begin frame source to ensure that callbacks called after
// all BeginFrames are sent.
//
// Lifetime: WebView
class BeginFrameSourceWebView : public viz::ExternalBeginFrameSource {
 public:
  BeginFrameSourceWebView();
  ~BeginFrameSourceWebView() override;

  // Sets parent of this BeginFrameSource
  void SetParentSource(BeginFrameSourceWebView* parent);
  bool inside_begin_frame() { return inside_begin_frame_; }

  // Schedules BeginFrame completion callback on root begin frame source.
  virtual void AddBeginFrameCompletionCallback(base::OnceClosure callback);

  // Returns last dispatched begin frame args.
  const viz::BeginFrameArgs& LastDispatchedBeginFrameArgs();

 protected:
  void ObserveBeginFrameSource(viz::BeginFrameSource* begin_frame_source);

  virtual void AfterBeginFrame() {}

 private:
  class BeginFrameObserver;
  class BeginFrameSourceClient : public viz::ExternalBeginFrameSourceClient {
   public:
    BeginFrameSourceClient(BeginFrameSourceWebView* owner);
    ~BeginFrameSourceClient();

    // ExternalBeginFrameSourceClient implementation.
    void OnNeedsBeginFrames(bool needs_begin_frames) override;

   private:
    const raw_ptr<BeginFrameSourceWebView> owner_;
  };

  void SendBeginFrame(const viz::BeginFrameArgs& args);
  void OnNeedsBeginFrames(bool needs_begin_frames);

  BeginFrameSourceClient bfs_client_;
  raw_ptr<viz::BeginFrameSource> observed_begin_frame_source_ = nullptr;
  raw_ptr<BeginFrameSourceWebView> parent_ = nullptr;
  std::unique_ptr<BeginFrameObserver> parent_observer_;
  bool inside_begin_frame_ = false;
};

// RootBeginFrameSourceWebView is subclass of BeginFrameSourceWebView that
// observes ExternalBeginFrameSourceAndroid to provide actual BeginFrames from
// Android Choreographer and implements the logic of
// AddBeginFrameCompletionCallback.
//
// Lifetime: Singleton
//
// There is only one RootBeginFrameSourceWebView, even if there are multiple
// displays with different VSync timings attached. Choreographer only uses the
// built-in display for frame timing.
class RootBeginFrameSourceWebView : public BeginFrameSourceWebView {
 public:
  static RootBeginFrameSourceWebView* GetInstance();

  void OnUpdateRefreshRate(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           float refresh_rate);

  // As this is implementation of root BeginFrameSourceWebView this is actual
  // implementation of scheduling callbacks.
  void AddBeginFrameCompletionCallback(base::OnceClosure callback) override;

 private:
  friend class base::NoDestructor<RootBeginFrameSourceWebView>;
  friend class BeginFrameSourceWebViewTest;

  RootBeginFrameSourceWebView();
  ~RootBeginFrameSourceWebView() override;

  void AfterBeginFrame() override;

  viz::ExternalBeginFrameSourceAndroid begin_frame_source_;
  std::vector<base::ScopedClosureRunner> after_begin_frame_callbacks_;
  base::android::ScopedJavaGlobalRef<jobject> j_object_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_BEGIN_FRAME_SOURCE_WEBVIEW_H_
