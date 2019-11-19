// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include <stdint.h>

#include <sstream>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/timer/lap_timer.h"
#include "cc/layers/nine_patch_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/texture_layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/layer_tree_json_parser.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/test_layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/test/paths.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "testing/perf/perf_result_reporter.h"

namespace cc {
namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class LayerTreeHostPerfTest : public LayerTreeTest {
 public:
  LayerTreeHostPerfTest()
      : draw_timer_(kWarmupRuns,
                    base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
                    kTimeCheckInterval),
        commit_timer_(0, base::TimeDelta(), 1),
        full_damage_each_frame_(false),
        begin_frame_driven_drawing_(false),
        measure_commit_cost_(false) {
  }

  std::unique_ptr<TestLayerTreeFrameSink> CreateLayerTreeFrameSink(
      const viz::RendererSettings& renderer_settings,
      double refresh_rate,
      scoped_refptr<viz::ContextProvider> compositor_context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider)
      override {
    constexpr bool disable_display_vsync = true;
    bool synchronous_composite =
        !HasImplThread() &&
        !layer_tree_host()->GetSettings().single_thread_proxy_scheduler;
    return std::make_unique<TestLayerTreeFrameSink>(
        compositor_context_provider, std::move(worker_context_provider),
        gpu_memory_buffer_manager(), renderer_settings, ImplThreadTaskRunner(),
        synchronous_composite, disable_display_vsync, refresh_rate);
  }

  void BeginTest() override {
    BuildTree();
    PostSetNeedsCommitToMainThread();
  }

  void BeginMainFrame(const viz::BeginFrameArgs& args) override {
    if (begin_frame_driven_drawing_ && !TestEnded())
      layer_tree_host()->SetNeedsCommitWithForcedRedraw();
  }

  void BeginCommitOnThread(LayerTreeHostImpl* host_impl) override {
    if (measure_commit_cost_)
      commit_timer_.Start();
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    if (measure_commit_cost_ && draw_timer_.IsWarmedUp()) {
      commit_timer_.NextLap();
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    if (TestEnded() || CleanUpStarted())
      return;
    draw_timer_.NextLap();
    if (draw_timer_.HasTimeLimitExpired()) {
      CleanUpAndEndTest();
      return;
    }
    if (!begin_frame_driven_drawing_)
      host_impl->SetNeedsRedraw();
    if (full_damage_each_frame_)
      host_impl->SetFullViewportDamage();
  }

  void SetUpReporter(const std::string& story_name) {
    reporter_ = std::make_unique<perf_test::PerfResultReporter>(
        "layer_tree_host", story_name);
    reporter_->RegisterImportantMetric("_frame_time", "us");
    reporter_->RegisterImportantMetric("_commit_time", "us");
  }

  virtual void CleanUpAndEndTest() { EndTest(); }

  virtual bool CleanUpStarted() { return false; }

  virtual void BuildTree() {}

  void AfterTest() override {
    CHECK(reporter_) << "Must SetUpReporter() before AfterTest().";
    reporter_->AddResult("_frame_time",
                         draw_timer_.TimePerLap().InMicrosecondsF());
    if (measure_commit_cost_) {
      reporter_->AddResult("_commit_time",
                           commit_timer_.TimePerLap().InMicrosecondsF());
    }
  }

 protected:
  base::LapTimer draw_timer_;
  base::LapTimer commit_timer_;

  std::unique_ptr<perf_test::PerfResultReporter> reporter_;
  FakeContentLayerClient fake_content_layer_client_;
  bool full_damage_each_frame_;
  bool begin_frame_driven_drawing_;

  bool measure_commit_cost_;
};


class LayerTreeHostPerfTestJsonReader : public LayerTreeHostPerfTest {
 public:
  LayerTreeHostPerfTestJsonReader()
      : LayerTreeHostPerfTest() {
  }

  void ReadTestFile(const std::string& name) {
    base::FilePath test_data_dir;
    ASSERT_TRUE(
        base::PathService::Get(viz::Paths::DIR_TEST_DATA, &test_data_dir));
    base::FilePath json_file = test_data_dir.AppendASCII(name + ".json");
    ASSERT_TRUE(base::ReadFileToString(json_file, &json_));
  }

  void BuildTree() override {
    gfx::Size viewport = gfx::Size(720, 1038);
    layer_tree_host()->SetViewportRectAndScale(gfx::Rect(viewport), 1.f,
                                               viz::LocalSurfaceIdAllocation());
    scoped_refptr<Layer> root = ParseTreeFromJson(json_,
                                                  &fake_content_layer_client_);
    ASSERT_TRUE(root.get());
    layer_tree_host()->SetRootLayer(root);
    fake_content_layer_client_.set_bounds(viewport);
  }

 private:
  std::string json_;
};

// Simulates a tab switcher scene with two stacks of 10 tabs each.
// Timed out on Android: http://crbug.com/723821
#if defined(OS_ANDROID)
#define MAYBE_TenTenSingleThread DISABLED_TenTenSingleThread
#else
#define MAYBE_TenTenSingleThread TenTenSingleThread
#endif
TEST_F(LayerTreeHostPerfTestJsonReader, MAYBE_TenTenSingleThread) {
  SetUpReporter("10_10_single_thread");
  ReadTestFile("10_10_layer_tree");
  RunTest(CompositorMode::SINGLE_THREADED);
}

// Timed out on Android: http://crbug.com/723821
#if defined(OS_ANDROID)
#define MAYBE_TenTenThreaded DISABLED_TenTenThreaded
#else
#define MAYBE_TenTenThreaded TenTenThreaded
#endif
TEST_F(LayerTreeHostPerfTestJsonReader, MAYBE_TenTenThreaded) {
  SetUpReporter("10_10_threaded_impl_side");
  ReadTestFile("10_10_layer_tree");
  RunTest(CompositorMode::THREADED);
}

// Simulates a tab switcher scene with two stacks of 10 tabs each.
TEST_F(LayerTreeHostPerfTestJsonReader,
       TenTenSingleThread_FullDamageEachFrame) {
  full_damage_each_frame_ = true;
  SetUpReporter("10_10_single_thread_full_damage_each_frame");
  ReadTestFile("10_10_layer_tree");
  RunTest(CompositorMode::SINGLE_THREADED);
}

TEST_F(LayerTreeHostPerfTestJsonReader, TenTenThreaded_FullDamageEachFrame) {
  full_damage_each_frame_ = true;
  SetUpReporter("10_10_threaded_impl_side_full_damage_each_frame");
  ReadTestFile("10_10_layer_tree");
  RunTest(CompositorMode::THREADED);
}

// Invalidates a leaf layer in the tree on the main thread after every commit.
class LayerTreeHostPerfTestLeafInvalidates
    : public LayerTreeHostPerfTestJsonReader {
 public:
  void BuildTree() override {
    LayerTreeHostPerfTestJsonReader::BuildTree();

    // Find a leaf layer.
    for (layer_to_invalidate_ = layer_tree_host()->root_layer();
         layer_to_invalidate_->children().size();
         layer_to_invalidate_ = layer_to_invalidate_->children()[0].get()) {
    }
  }

  void DidCommitAndDrawFrame() override {
    if (TestEnded())
      return;

    layer_to_invalidate_->SetOpacity(
        layer_to_invalidate_->opacity() != 1.f ? 1.f : 0.5f);
  }

 protected:
  Layer* layer_to_invalidate_;
};

// Simulates a tab switcher scene with two stacks of 10 tabs each. Invalidate a
// property on a leaf layer in the tree every commit.
TEST_F(LayerTreeHostPerfTestLeafInvalidates, TenTenSingleThread) {
  SetUpReporter("10_10_single_thread_leaf_invalidates");
  ReadTestFile("10_10_layer_tree");
  RunTest(CompositorMode::SINGLE_THREADED);
}

// Timed out on Android: http://crbug.com/723821
TEST_F(LayerTreeHostPerfTestLeafInvalidates, MAYBE_TenTenThreaded) {
  SetUpReporter("10_10_threaded_impl_side_leaf_invalidates");
  ReadTestFile("10_10_layer_tree");
  RunTest(CompositorMode::THREADED);
}

// Simulates main-thread scrolling on each frame.
class ScrollingLayerTreePerfTest : public LayerTreeHostPerfTestJsonReader {
 public:
  ScrollingLayerTreePerfTest()
      : LayerTreeHostPerfTestJsonReader() {
  }

  void BuildTree() override {
    LayerTreeHostPerfTestJsonReader::BuildTree();
    scrollable_ = layer_tree_host()->root_layer()->children()[1];
    ASSERT_TRUE(scrollable_.get());
  }

  void UpdateLayerTreeHost() override {
    if (TestEnded())
      return;
    static const gfx::Vector2d delta = gfx::Vector2d(0, 10);
    scrollable_->SetScrollOffset(
        gfx::ScrollOffsetWithDelta(scrollable_->CurrentScrollOffset(), delta));
  }

 private:
  scoped_refptr<Layer> scrollable_;
};

// Timed out on Android: http://crbug.com/723821
#if defined(OS_ANDROID)
#define MAYBE_LongScrollablePageSingleThread \
    DISABLED_LongScrollablePageSingleThread
#else
#define MAYBE_LongScrollablePageSingleThread LongScrollablePageSingleThread
#endif
TEST_F(ScrollingLayerTreePerfTest, MAYBE_LongScrollablePageSingleThread) {
  SetUpReporter("long_scrollable_page");
  ReadTestFile("long_scrollable_page");
  RunTest(CompositorMode::SINGLE_THREADED);
}

// Timed out on Android: http://crbug.com/723821
#if defined(OS_ANDROID)
#define MAYBE_LongScrollablePageThreaded DISABLED_LongScrollablePageThreaded
#else
#define MAYBE_LongScrollablePageThreaded LongScrollablePageThreaded
#endif
TEST_F(ScrollingLayerTreePerfTest, MAYBE_LongScrollablePageThreaded) {
  SetUpReporter("long_scrollable_page_threaded_impl_side");
  ReadTestFile("long_scrollable_page");
  RunTest(CompositorMode::THREADED);
}

// Simulates main-thread scrolling on each frame.
class BrowserCompositorInvalidateLayerTreePerfTest
    : public LayerTreeHostPerfTestJsonReader {
 public:
  BrowserCompositorInvalidateLayerTreePerfTest() = default;

  void BuildTree() override {
    LayerTreeHostPerfTestJsonReader::BuildTree();
    tab_contents_ = static_cast<TextureLayer*>(layer_tree_host()
                                                   ->root_layer()
                                                   ->children()[0]
                                                   ->children()[0]
                                                   ->children()[0]
                                                   ->children()[0]
                                                   .get());
    ASSERT_TRUE(tab_contents_.get());
  }

  void WillCommit() override {
    if (CleanUpStarted())
      return;
    gpu::Mailbox gpu_mailbox;
    std::ostringstream name_stream;
    name_stream << "name" << next_fence_sync_;
    gpu_mailbox.SetName(
        reinterpret_cast<const int8_t*>(name_stream.str().c_str()));
    std::unique_ptr<viz::SingleReleaseCallback> callback =
        viz::SingleReleaseCallback::Create(base::BindOnce(
            &BrowserCompositorInvalidateLayerTreePerfTest::ReleaseMailbox,
            base::Unretained(this)));

    gpu::SyncToken next_sync_token(gpu::CommandBufferNamespace::GPU_IO,
                                   gpu::CommandBufferId::FromUnsafeValue(1),
                                   next_fence_sync_);
    next_sync_token.SetVerifyFlush();

    constexpr gfx::Size size(64, 64);
    viz::TransferableResource resource = viz::TransferableResource::MakeGL(
        gpu_mailbox, GL_LINEAR, GL_TEXTURE_2D, next_sync_token, size,
        false /* is_overlay_candidate */);
    next_fence_sync_++;

    tab_contents_->SetTransferableResource(resource, std::move(callback));
    ++sent_mailboxes_count_;
    tab_contents_->SetNeedsDisplay();
  }

  void DidCommit() override {
    if (CleanUpStarted())
      return;
    layer_tree_host()->SetNeedsCommit();
  }

  void CleanUpAndEndTest() override {
    clean_up_started_ = true;
    MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&BrowserCompositorInvalidateLayerTreePerfTest::
                           CleanUpAndEndTestOnMainThread,
                       base::Unretained(this)));
  }

  void CleanUpAndEndTestOnMainThread() {
    tab_contents_->ClearTexture();
    // ReleaseMailbox will end the test when we get the last mailbox back.
  }

  void ReleaseMailbox(const gpu::SyncToken& sync_token, bool lost_resource) {
    ++released_mailboxes_count_;
    if (released_mailboxes_count_ == sent_mailboxes_count_) {
      DCHECK(CleanUpStarted());
      EndTest();
    }
  }

  bool CleanUpStarted() override { return clean_up_started_; }

 private:
  scoped_refptr<TextureLayer> tab_contents_;
  uint64_t next_fence_sync_ = 1;
  bool clean_up_started_ = false;
  int sent_mailboxes_count_ = 0;
  int released_mailboxes_count_ = 0;
};

TEST_F(BrowserCompositorInvalidateLayerTreePerfTest, DenseBrowserUIThreaded) {
  measure_commit_cost_ = true;
  SetUpReporter("dense_layer_tree");
  ReadTestFile("dense_layer_tree");
  RunTest(CompositorMode::THREADED);
}

// Simulates a page with several large, transformed and animated layers.
// Timed out on Android: http://crbug.com/723821
#if defined(OS_ANDROID)
#define MAYBE_HeavyPageThreaded DISABLED_HeavyPageThreaded
#else
#define MAYBE_HeavyPageThreaded HeavyPageThreaded
#endif
TEST_F(LayerTreeHostPerfTestJsonReader, MAYBE_HeavyPageThreaded) {
  begin_frame_driven_drawing_ = true;
  measure_commit_cost_ = true;
  SetUpReporter("heavy_page");
  ReadTestFile("heavy_layer_tree");
  RunTest(CompositorMode::THREADED);
}

}  // namespace
}  // namespace cc
