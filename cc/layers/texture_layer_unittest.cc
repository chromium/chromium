// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/texture_layer.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "cc/animation/animation_host.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/texture_layer_client.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/stub_layer_tree_host_single_thread_client.h"
#include "cc/test/test_layer_tree_frame_sink.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/test/fake_output_surface.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Mock;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;

namespace cc {
namespace {

gpu::Mailbox MailboxFromChar(char value) {
  gpu::Mailbox mailbox;
  memset(mailbox.name, value, sizeof(mailbox.name));
  return mailbox;
}

gpu::SyncToken SyncTokenFromUInt(uint32_t value) {
  return gpu::SyncToken(gpu::CommandBufferNamespace::GPU_IO,
                        gpu::CommandBufferId::FromUnsafeValue(0x123), value);
}

class MockLayerTreeHost : public LayerTreeHost {
 public:
  static std::unique_ptr<MockLayerTreeHost> Create(
      FakeLayerTreeHostClient* client,
      TaskGraphRunner* task_graph_runner,
      MutatorHost* mutator_host) {
    LayerTreeHost::InitParams params;
    params.client = client;
    params.task_graph_runner = task_graph_runner;
    params.mutator_host = mutator_host;
    LayerTreeSettings settings;
    params.settings = &settings;
    return base::WrapUnique(new MockLayerTreeHost(std::move(params)));
  }

  MOCK_METHOD0(SetNeedsCommit, void());
  MOCK_METHOD0(StartRateLimiter, void());
  MOCK_METHOD0(StopRateLimiter, void());

 private:
  explicit MockLayerTreeHost(LayerTreeHost::InitParams params)
      : LayerTreeHost(std::move(params), CompositorMode::SINGLE_THREADED) {
    InitializeSingleThreaded(&single_thread_client_,
                             base::ThreadTaskRunnerHandle::Get());
  }

  StubLayerTreeHostSingleThreadClient single_thread_client_;
};

class MockReleaseCallback {
 public:
  MOCK_METHOD3(Release,
               void(const gpu::Mailbox& mailbox,
                    const gpu::SyncToken& sync_token,
                    bool lost_resource));
  MOCK_METHOD3(Release2,
               void(const viz::SharedBitmapId& shared_bitmap_id,
                    const gpu::SyncToken& sync_token,
                    bool lost_resource));
};

struct CommonResourceObjects {
  explicit CommonResourceObjects(viz::SharedBitmapManager* manager)
      : mailbox_name1_(MailboxFromChar('1')),
        mailbox_name2_(MailboxFromChar('2')),
        sync_token1_(gpu::CommandBufferNamespace::GPU_IO,
                     gpu::CommandBufferId::FromUnsafeValue(0x234),
                     1),
        sync_token2_(gpu::CommandBufferNamespace::GPU_IO,
                     gpu::CommandBufferId::FromUnsafeValue(0x234),
                     2) {
    release_callback1_ =
        base::BindRepeating(&MockReleaseCallback::Release,
                            base::Unretained(&mock_callback_), mailbox_name1_);
    release_callback2_ =
        base::BindRepeating(&MockReleaseCallback::Release,
                            base::Unretained(&mock_callback_), mailbox_name2_);
    const uint32_t arbitrary_target1 = GL_TEXTURE_2D;
    const uint32_t arbitrary_target2 = GL_TEXTURE_EXTERNAL_OES;
    gfx::Size size(128, 128);
    resource1_ = viz::TransferableResource::MakeGL(
        mailbox_name1_, GL_LINEAR, arbitrary_target1, sync_token1_, size,
        false /* is_overlay_candidate */);
    resource2_ = viz::TransferableResource::MakeGL(
        mailbox_name2_, GL_LINEAR, arbitrary_target2, sync_token2_, size,
        false /* is_overlay_candidate */);
    shared_bitmap_id_ = viz::SharedBitmap::GenerateId();
    sw_release_callback_ = base::BindRepeating(
        &MockReleaseCallback::Release2, base::Unretained(&mock_callback_),
        shared_bitmap_id_);
    sw_resource_ = viz::TransferableResource::MakeSoftware(
        shared_bitmap_id_, size, viz::RGBA_8888);
  }

  using RepeatingReleaseCallback =
      base::RepeatingCallback<void(const gpu::SyncToken& sync_token,
                                   bool is_lost)>;

  gpu::Mailbox mailbox_name1_;
  gpu::Mailbox mailbox_name2_;
  MockReleaseCallback mock_callback_;
  RepeatingReleaseCallback release_callback1_;
  RepeatingReleaseCallback release_callback2_;
  RepeatingReleaseCallback sw_release_callback_;
  gpu::SyncToken sync_token1_;
  gpu::SyncToken sync_token2_;
  viz::SharedBitmapId shared_bitmap_id_;
  viz::TransferableResource resource1_;
  viz::TransferableResource resource2_;
  viz::TransferableResource sw_resource_;
};

class TextureLayerTest : public testing::Test {
 public:
  TextureLayerTest()
      : layer_tree_frame_sink_(FakeLayerTreeFrameSink::Create3d()),
        host_impl_(&task_runner_provider_, &task_graph_runner_),
        test_data_(&shared_bitmap_manager_) {}

 protected:
  void SetUp() override {
    animation_host_ = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
    layer_tree_host_ = MockLayerTreeHost::Create(
        &fake_client_, &task_graph_runner_, animation_host_.get());
    EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());
    layer_tree_host_->SetViewportRectAndScale(gfx::Rect(10, 10), 1.f,
                                              viz::LocalSurfaceIdAllocation());
    Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  }

  void TearDown() override {
    Mock::VerifyAndClearExpectations(layer_tree_host_.get());
    EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());

    animation_host_->SetMutatorHostClient(nullptr);
    layer_tree_host_->SetRootLayer(nullptr);
    layer_tree_host_ = nullptr;
    animation_host_ = nullptr;
  }

  std::unique_ptr<MockLayerTreeHost> layer_tree_host_;
  std::unique_ptr<AnimationHost> animation_host_;
  FakeImplTaskRunnerProvider task_runner_provider_;
  FakeLayerTreeHostClient fake_client_;
  viz::TestSharedBitmapManager shared_bitmap_manager_;
  TestTaskGraphRunner task_graph_runner_;
  std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink_;
  FakeLayerTreeHostImpl host_impl_;
  CommonResourceObjects test_data_;
};

TEST_F(TextureLayerTest, CheckPropertyChangeCausesCorrectBehavior) {
  scoped_refptr<TextureLayer> test_layer =
      TextureLayer::CreateForMailbox(nullptr);
  EXPECT_SET_NEEDS_COMMIT(1, layer_tree_host_->SetRootLayer(test_layer));

  // Test properties that should call SetNeedsCommit.  All properties need to
  // be set to new values in order for SetNeedsCommit to be called.
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetFlipped(false));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetNearestNeighbor(true));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetUV(
      gfx::PointF(0.25f, 0.25f), gfx::PointF(0.75f, 0.75f)));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetVertexOpacity(
      0.5f, 0.5f, 0.5f, 0.5f));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetPremultipliedAlpha(false));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetBlendBackgroundColor(true));
}

class RunOnCommitLayerTreeHostClient : public FakeLayerTreeHostClient {
 public:
  void set_run_on_commit_and_draw(base::OnceClosure c) {
    run_on_commit_and_draw_ = std::move(c);
  }

  void DidCommitAndDrawFrame() override {
    if (run_on_commit_and_draw_)
      std::move(run_on_commit_and_draw_).Run();
  }

 private:
  base::OnceClosure run_on_commit_and_draw_;
};

// If the compositor is destroyed while TextureLayer has a resource in it, the
// resource should be returned to the client. https://crbug.com/857262
TEST_F(TextureLayerTest, ShutdownWithResource) {
  for (int i = 0; i < 2; ++i) {
    bool gpu = i == 0;
    SCOPED_TRACE(gpu);
    // Make our own LayerTreeHost for this test so we can control the lifetime.
    StubLayerTreeHostSingleThreadClient single_thread_client;
    RunOnCommitLayerTreeHostClient client;
    LayerTreeHost::InitParams params;
    params.client = &client;
    params.task_graph_runner = &task_graph_runner_;
    params.mutator_host = animation_host_.get();
    LayerTreeSettings settings;
    params.settings = &settings;
    params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
    auto host = LayerTreeHost::CreateSingleThreaded(&single_thread_client,
                                                    std::move(params));

    client.SetLayerTreeHost(host.get());
    client.SetUseSoftwareCompositing(!gpu);

    scoped_refptr<TextureLayer> layer = TextureLayer::CreateForMailbox(nullptr);
    layer->SetIsDrawable(true);
    layer->SetBounds(gfx::Size(10, 10));
    if (gpu) {
      layer->SetTransferableResource(
          test_data_.resource1_,
          viz::SingleReleaseCallback::Create(test_data_.release_callback1_));
    } else {
      layer->SetTransferableResource(
          test_data_.sw_resource_,
          viz::SingleReleaseCallback::Create(test_data_.sw_release_callback_));
    }

    viz::ParentLocalSurfaceIdAllocator allocator;
    allocator.GenerateId();
    host->SetViewportRectAndScale(
        gfx::Rect(10, 10), 1.f, allocator.GetCurrentLocalSurfaceIdAllocation());
    host->SetVisible(true);
    host->SetRootLayer(layer);

    // Commit and activate the TransferableResource in the TextureLayer.
    {
      base::RunLoop loop;
      client.set_run_on_commit_and_draw(loop.QuitClosure());
      loop.Run();
    }

    // Destroy the LayerTreeHost and the compositor-thread LayerImpl trees
    // while the resource is still in the layer. The resource should be released
    // back to the TextureLayer's client, but is post-tasked back so...
    host = nullptr;

    // We have to wait for the posted ReleaseCallback to run.
    // Our LayerTreeHostClient makes a FakeLayerTreeFrameSink which returns all
    // resources when its detached, so the resources will not be in use in the
    // display compositor, and will be returned as not lost.
    if (gpu) {
      EXPECT_CALL(test_data_.mock_callback_,
                  Release(test_data_.mailbox_name1_, _, false))
          .Times(1);
    } else {
      EXPECT_CALL(test_data_.mock_callback_,
                  Release2(test_data_.shared_bitmap_id_, _, false))
          .Times(1);
    }
    {
      base::RunLoop loop;
      loop.RunUntilIdle();
    }
  }
}

class TestMailboxHolder : public TextureLayer::TransferableResourceHolder {
 public:
  using TextureLayer::TransferableResourceHolder::Create;

 protected:
  ~TestMailboxHolder() override = default;
};

class TextureLayerWithResourceTest : public TextureLayerTest {
 protected:
  void TearDown() override {
    Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);
    EXPECT_CALL(
        test_data_.mock_callback_,
        Release(test_data_.mailbox_name1_, test_data_.sync_token1_, false))
        .Times(1);
    TextureLayerTest::TearDown();
  }
};

TEST_F(TextureLayerWithResourceTest, ReplaceMailboxOnMainThreadBeforeCommit) {
  scoped_refptr<TextureLayer> test_layer =
      TextureLayer::CreateForMailbox(nullptr);
  ASSERT_TRUE(test_layer.get());

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());
  layer_tree_host_->SetRootLayer(test_layer);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTransferableResource(
      test_data_.resource1_,
      viz::SingleReleaseCallback::Create(test_data_.release_callback1_));
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  EXPECT_CALL(
      test_data_.mock_callback_,
      Release(test_data_.mailbox_name1_, test_data_.sync_token1_, false))
      .Times(1);
  test_layer->SetTransferableResource(
      test_data_.resource2_,
      viz::SingleReleaseCallback::Create(test_data_.release_callback2_));
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  EXPECT_CALL(
      test_data_.mock_callback_,
      Release(test_data_.mailbox_name2_, test_data_.sync_token2_, false))
      .Times(1);
  test_layer->ClearTexture();
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTransferableResource(
      test_data_.sw_resource_,
      viz::SingleReleaseCallback::Create(test_data_.sw_release_callback_));
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  EXPECT_CALL(test_data_.mock_callback_,
              Release2(test_data_.shared_bitmap_id_, _, false))
      .Times(1);
  test_layer->ClearTexture();
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // Test destructor.
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTransferableResource(
      test_data_.resource1_,
      viz::SingleReleaseCallback::Create(test_data_.release_callback1_));
}

class TextureLayerMailboxHolderTest : public TextureLayerTest {
 public:
  TextureLayerMailboxHolderTest()
      : main_thread_("MAIN") {
    main_thread_.Start();
  }

  void Wait(const base::Thread& thread) {
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    thread.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&event)));
    event.Wait();
  }

  void CreateMainRef() {
    main_ref_ = TestMailboxHolder::Create(
        test_data_.resource1_,
        viz::SingleReleaseCallback::Create(test_data_.release_callback1_));
  }

  void ReleaseMainRef() { main_ref_ = nullptr; }

  void CreateImplRef(
      std::unique_ptr<viz::SingleReleaseCallback>* impl_ref,
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner) {
    *impl_ref = main_ref_->holder()->GetCallbackForImplThread(
        std::move(main_thread_task_runner));
  }

 protected:
  std::unique_ptr<TestMailboxHolder::MainThreadReference> main_ref_;
  base::Thread main_thread_;
};

TEST_F(TextureLayerMailboxHolderTest, TwoCompositors_BothReleaseThenMain) {
  scoped_refptr<TextureLayer> test_layer =
      TextureLayer::CreateForMailbox(nullptr);
  ASSERT_TRUE(test_layer.get());

  main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TextureLayerMailboxHolderTest::CreateMainRef,
                                base::Unretained(this)));

  Wait(main_thread_);

  // The texture layer is attached to compositor1, and passes a reference to its
  // impl tree.
  std::unique_ptr<viz::SingleReleaseCallback> compositor1;
  main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TextureLayerMailboxHolderTest::CreateImplRef,
                                base::Unretained(this), &compositor1,
                                main_thread_.task_runner()));

  // Then the texture layer is removed and attached to compositor2, and passes a
  // reference to its impl tree.
  std::unique_ptr<viz::SingleReleaseCallback> compositor2;
  main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TextureLayerMailboxHolderTest::CreateImplRef,
                                base::Unretained(this), &compositor2,
                                main_thread_.task_runner()));

  Wait(main_thread_);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // The compositors both destroy their impl trees before the main thread layer
  // is destroyed.
  compositor1->Run(SyncTokenFromUInt(100), false);
  compositor2->Run(SyncTokenFromUInt(200), false);

  Wait(main_thread_);

  EXPECT_CALL(test_data_.mock_callback_, Release(_, _, _)).Times(0);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // The main thread ref is the last one, so the resource is released back to
  // the embedder, with the last sync point provided by the impl trees.
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, SyncTokenFromUInt(200), false))
      .Times(1);

  main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TextureLayerMailboxHolderTest::ReleaseMainRef,
                                base::Unretained(this)));
  Wait(main_thread_);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);
}

TEST_F(TextureLayerMailboxHolderTest, TwoCompositors_MainReleaseBetween) {
  scoped_refptr<TextureLayer> test_layer =
      TextureLayer::CreateForMailbox(nullptr);
  ASSERT_TRUE(test_layer.get());

  main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TextureLayerMailboxHolderTest::CreateMainRef,
                                base::Unretained(this)));

  Wait(main_thread_);

  // The texture layer is attached to compositor1, and passes a reference to its
  // impl tree.
  std::unique_ptr<viz::SingleReleaseCallback> compositor1;
  main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TextureLayerMailboxHolderTest::CreateImplRef,
                                base::Unretained(this), &compositor1,
                                main_thread_.task_runner()));

  // Then the texture layer is removed and attached to compositor2, and passes a
  // reference to its impl tree.
  std::unique_ptr<viz::SingleReleaseCallback> compositor2;
  main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TextureLayerMailboxHolderTest::CreateImplRef,
                                base::Unretained(this), &compositor2,
                                main_thread_.task_runner()));

  Wait(main_thread_);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // One compositor destroys their impl tree.
  compositor1->Run(SyncTokenFromUInt(100), false);

  // Then the main thread reference is destroyed.
  main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TextureLayerMailboxHolderTest::ReleaseMainRef,
                                base::Unretained(this)));

  Wait(main_thread_);

  EXPECT_CALL(test_data_.mock_callback_, Release(_, _, _)).Times(0);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // The second impl reference is destroyed last, causing the resource to be
  // released back to the embedder with the last sync point from the impl tree.
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, SyncTokenFromUInt(200), true))
      .Times(1);

  compositor2->Run(SyncTokenFromUInt(200), true);
  Wait(main_thread_);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);
}

TEST_F(TextureLayerMailboxHolderTest, TwoCompositors_MainReleasedFirst) {
  scoped_refptr<TextureLayer> test_layer =
      TextureLayer::CreateForMailbox(nullptr);
  ASSERT_TRUE(test_layer.get());

  main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TextureLayerMailboxHolderTest::CreateMainRef,
                                base::Unretained(this)));

  Wait(main_thread_);

  // The texture layer is attached to compositor1, and passes a reference to its
  // impl tree.
  std::unique_ptr<viz::SingleReleaseCallback> compositor1;
  main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TextureLayerMailboxHolderTest::CreateImplRef,
                                base::Unretained(this), &compositor1,
                                main_thread_.task_runner()));

  // Then the texture layer is removed and attached to compositor2, and passes a
  // reference to its impl tree.
  std::unique_ptr<viz::SingleReleaseCallback> compositor2;
  main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TextureLayerMailboxHolderTest::CreateImplRef,
                                base::Unretained(this), &compositor2,
                                main_thread_.task_runner()));

  Wait(main_thread_);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // The main thread reference is destroyed first.
  main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TextureLayerMailboxHolderTest::ReleaseMainRef,
                                base::Unretained(this)));

  // One compositor destroys their impl tree.
  compositor2->Run(SyncTokenFromUInt(200), false);

  Wait(main_thread_);

  EXPECT_CALL(test_data_.mock_callback_, Release(_, _, _)).Times(0);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // The second impl reference is destroyed last, causing the resource to be
  // released back to the embedder with the last sync point from the impl tree.
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, SyncTokenFromUInt(100), true))
      .Times(1);

  compositor1->Run(SyncTokenFromUInt(100), true);
  Wait(main_thread_);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);
}

class TextureLayerImplWithMailboxThreadedCallback : public LayerTreeTest {
 public:
  std::unique_ptr<TestLayerTreeFrameSink> CreateLayerTreeFrameSink(
      const viz::RendererSettings& renderer_settings,
      double refresh_rate,
      scoped_refptr<viz::ContextProvider> compositor_context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider)
      override {
    constexpr bool disable_display_vsync = false;
    bool synchronous_composite =
        !HasImplThread() &&
        !layer_tree_host()->GetSettings().single_thread_proxy_scheduler;
    return std::make_unique<TestLayerTreeFrameSink>(
        compositor_context_provider, std::move(worker_context_provider),
        gpu_memory_buffer_manager(), renderer_settings, ImplThreadTaskRunner(),
        synchronous_composite, disable_display_vsync, refresh_rate);
  }

  void AdvanceTestCase() {
    ++test_case_;
    switch (test_case_) {
      case 1:
        // Case #1: change resource before the commit. The old resource should
        // be released immediately.
        SetMailbox('2');
        EXPECT_EQ(1, callback_count_);
        PostSetNeedsCommitToMainThread();

        // Case 2 does not rely on callbacks to advance.
        pending_callback_ = false;
        break;
      case 2:
        // Case #2: change resource after the commit (and draw), where the
        // layer draws. The old resource should be released during the next
        // commit.
        SetMailbox('3');
        EXPECT_EQ(1, callback_count_);

        // Cases 3-5 rely on a callback to advance.
        pending_callback_ = true;
        break;
      case 3:
        EXPECT_EQ(2, callback_count_);
        // Case #3: change resource when the layer doesn't draw. The old
        // resource should be released during the next commit.
        layer_->SetBounds(gfx::Size());
        SetMailbox('4');
        break;
      case 4:
        EXPECT_EQ(3, callback_count_);
        // Case #4: release resource that was committed but never drawn. The
        // old resource should be released during the next commit.
        layer_->ClearTexture();
        break;
      case 5:
        EXPECT_EQ(4, callback_count_);
        // Restore a resource for the next step.
        SetMailbox('5');

        // Cases 6 and 7 do not rely on callbacks to advance.
        pending_callback_ = false;
        break;
      case 6:
        // Case #5: remove layer from tree. Callback should *not* be called, the
        // resource is returned to the main thread.
        EXPECT_EQ(4, callback_count_);
        layer_->RemoveFromParent();
        break;
      case 7:
        EXPECT_EQ(4, callback_count_);
        // Resetting the resource will call the callback now, before another
        // commit is needed, as the ReleaseCallback is already in flight from
        // RemoveFromParent().
        layer_->ClearTexture();
        pending_callback_ = true;
        frame_number_ = layer_tree_host()->SourceFrameNumber();
        break;
      case 8:
        // A commit wasn't needed, the ReleaseCallback was already in flight.
        EXPECT_EQ(frame_number_, layer_tree_host()->SourceFrameNumber());
        EXPECT_EQ(5, callback_count_);
        EndTest();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  // Make sure callback is received on main and doesn't block the impl thread.
  void ReleaseCallback(char mailbox_char,
                       const gpu::SyncToken& sync_token,
                       bool lost_resource) {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());
    EXPECT_FALSE(lost_resource);
    ++callback_count_;

    // If we are waiting on a callback, advance now.
    if (pending_callback_)
      AdvanceTestCase();
  }

  void SetMailbox(char mailbox_char) {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());
    std::unique_ptr<viz::SingleReleaseCallback> callback =
        viz::SingleReleaseCallback::Create(base::BindOnce(
            &TextureLayerImplWithMailboxThreadedCallback::ReleaseCallback,
            base::Unretained(this), mailbox_char));

    const gfx::Size size(64, 64);
    auto resource = viz::TransferableResource::MakeGL(
        MailboxFromChar(mailbox_char), GL_LINEAR, GL_TEXTURE_2D,
        SyncTokenFromUInt(static_cast<uint32_t>(mailbox_char)), size,
        false /* is_overlay_candidate */);
    layer_->SetTransferableResource(resource, std::move(callback));
    // Damage the layer so we send a new frame with the new resource to the
    // Display compositor.
    layer_->SetNeedsDisplay();
  }

  void BeginTest() override {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());

    gfx::Size bounds(100, 100);
    root_ = Layer::Create();
    root_->SetBounds(bounds);

    layer_ = TextureLayer::CreateForMailbox(nullptr);
    layer_->SetIsDrawable(true);
    layer_->SetBounds(bounds);

    root_->AddChild(layer_);
    layer_tree_host()->SetRootLayer(root_);
    layer_tree_host()->SetViewportRectAndScale(gfx::Rect(bounds), 1.f,
                                               viz::LocalSurfaceIdAllocation());
    SetMailbox('1');
    EXPECT_EQ(0, callback_count_);

    // Setup is complete - advance to test case 1.
    AdvanceTestCase();
  }

  void DidCommit() override {
    // If we are not waiting on a callback, advance now.
    if (!pending_callback_)
      AdvanceTestCase();
  }

 private:
  base::ThreadChecker main_thread_;
  int callback_count_ = 0;
  int test_case_ = 0;
  int frame_number_ = 0;
  // Whether we are waiting on a callback to advance the test case.
  bool pending_callback_ = false;
  scoped_refptr<Layer> root_;
  scoped_refptr<TextureLayer> layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(TextureLayerImplWithMailboxThreadedCallback);

class TextureLayerMailboxIsActivatedDuringCommit : public LayerTreeTest {
 protected:
  void ReleaseCallback(const gpu::SyncToken& original_sync_token,
                       const gpu::SyncToken& release_sync_token,
                       bool lost_resource) {
    released_count_++;
    switch (released_count_) {
      case 1:
        break;
      case 2:
        EXPECT_EQ(3, layer_tree_host()->SourceFrameNumber());
        EndTest();
        break;
      default:
        NOTREACHED();
    }
  }

  void SetMailbox(char mailbox_char) {
    const gpu::SyncToken sync_token =
        SyncTokenFromUInt(static_cast<uint32_t>(mailbox_char));
    std::unique_ptr<viz::SingleReleaseCallback> callback =
        viz::SingleReleaseCallback::Create(base::BindOnce(
            &TextureLayerMailboxIsActivatedDuringCommit::ReleaseCallback,
            base::Unretained(this), sync_token));
    constexpr gfx::Size size(64, 64);
    auto resource = viz::TransferableResource::MakeGL(
        MailboxFromChar(mailbox_char), GL_LINEAR, GL_TEXTURE_2D, sync_token,
        size, false /* is_overlay_candidate */);
    layer_->SetTransferableResource(resource, std::move(callback));
  }

  void BeginTest() override {
    gfx::Size bounds(100, 100);
    root_ = Layer::Create();
    root_->SetBounds(bounds);

    layer_ = TextureLayer::CreateForMailbox(nullptr);
    layer_->SetIsDrawable(true);
    layer_->SetBounds(bounds);

    root_->AddChild(layer_);
    layer_tree_host()->SetRootLayer(root_);
    layer_tree_host()->SetViewportRectAndScale(gfx::Rect(bounds), 1.f,
                                               viz::LocalSurfaceIdAllocation());
    SetMailbox('1');

    PostSetNeedsCommitToMainThread();
  }

  void WillActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    base::AutoLock lock(activate_count_lock_);
    ++activate_count_;
  }

  void DidCommit() override {
    // The first frame doesn't cause anything to be returned so it does not
    // need to wait for activation.
    if (layer_tree_host()->SourceFrameNumber() > 1) {
      base::AutoLock lock(activate_count_lock_);
      // The activate happened before commit is done on the main side.
      EXPECT_EQ(activate_count_, layer_tree_host()->SourceFrameNumber());
    }

    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1:
        // The first mailbox has been activated. Set a new mailbox, and
        // expect the next commit to finish *after* it is activated.
        SetMailbox('2');
        break;
      case 2:
        // The second mailbox has been activated. Remove the layer from
        // the tree to cause another commit/activation. The commit should
        // finish *after* the layer is removed from the active tree.
        layer_->RemoveFromParent();
        break;
      case 3:
        // This ensures all texture mailboxes are released before the end of the
        // test.
        layer_->ClearClient();
        break;
      default:
        NOTREACHED();
    }
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    // The activate didn't happen before commit is done on the impl side (but it
    // should happen before the main thread is done).
    EXPECT_EQ(activate_count_, host_impl->sync_tree()->source_frame_number());
  }

  base::Lock activate_count_lock_;
  int activate_count_ = 0;
  scoped_refptr<Layer> root_;
  scoped_refptr<TextureLayer> layer_;
  int released_count_ = 0;
};

SINGLE_AND_MULTI_THREAD_TEST_F(TextureLayerMailboxIsActivatedDuringCommit);

class TextureLayerImplWithResourceTest : public TextureLayerTest {
 protected:
  void SetUp() override {
    TextureLayerTest::SetUp();
    layer_tree_host_ = MockLayerTreeHost::Create(
        &fake_client_, &task_graph_runner_, animation_host_.get());
    host_impl_.SetVisible(true);
    EXPECT_TRUE(host_impl_.InitializeFrameSink(layer_tree_frame_sink_.get()));
  }

  std::unique_ptr<TextureLayerImpl> CreateTextureLayer() {
    auto layer = TextureLayerImpl::Create(host_impl_.active_tree(), 1);
    layer->set_visible_layer_rect(gfx::Rect(100, 100));
    return layer;
  }

  bool WillDraw(TextureLayerImpl* layer, DrawMode mode) {
    bool will_draw = layer->WillDraw(
        mode, host_impl_.active_tree()->resource_provider());
    if (will_draw)
      layer->DidDraw(host_impl_.active_tree()->resource_provider());
    return will_draw;
  }

  FakeLayerTreeHostClient fake_client_;
};

// Test conditions for results of TextureLayerImpl::WillDraw under
// different configurations of different mailbox, texture_id, and draw_mode.
TEST_F(TextureLayerImplWithResourceTest, TestWillDraw) {
  EXPECT_CALL(
      test_data_.mock_callback_,
      Release(test_data_.mailbox_name1_, test_data_.sync_token1_, false))
      .Times(AnyNumber());
  EXPECT_CALL(test_data_.mock_callback_,
              Release2(test_data_.shared_bitmap_id_, gpu::SyncToken(), false))
      .Times(AnyNumber());
  // Hardware mode.
  {
    std::unique_ptr<TextureLayerImpl> impl_layer = CreateTextureLayer();
    impl_layer->SetTransferableResource(
        test_data_.resource1_,
        viz::SingleReleaseCallback::Create(test_data_.release_callback1_));
    EXPECT_TRUE(WillDraw(impl_layer.get(), DRAW_MODE_HARDWARE));
  }

  {
    std::unique_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1);
    impl_layer->SetTransferableResource(viz::TransferableResource(), nullptr);
    EXPECT_FALSE(WillDraw(impl_layer.get(), DRAW_MODE_HARDWARE));
  }

  // Software mode.
  {
    std::unique_ptr<TextureLayerImpl> impl_layer = CreateTextureLayer();
    impl_layer->SetTransferableResource(
        test_data_.resource1_,
        viz::SingleReleaseCallback::Create(test_data_.release_callback1_));
    EXPECT_FALSE(WillDraw(impl_layer.get(), DRAW_MODE_SOFTWARE));
  }

  {
    std::unique_ptr<TextureLayerImpl> impl_layer = CreateTextureLayer();
    impl_layer->SetTransferableResource(viz::TransferableResource(), nullptr);
    EXPECT_FALSE(WillDraw(impl_layer.get(), DRAW_MODE_SOFTWARE));
  }

  {
    // Software resource.
    std::unique_ptr<TextureLayerImpl> impl_layer = CreateTextureLayer();
    impl_layer->SetTransferableResource(
        test_data_.sw_resource_,
        viz::SingleReleaseCallback::Create(test_data_.sw_release_callback_));
    EXPECT_TRUE(WillDraw(impl_layer.get(), DRAW_MODE_SOFTWARE));
  }

  // Resourceless software mode.
  {
    std::unique_ptr<TextureLayerImpl> impl_layer = CreateTextureLayer();
    impl_layer->SetTransferableResource(
        test_data_.resource1_,
        viz::SingleReleaseCallback::Create(test_data_.release_callback1_));
    EXPECT_FALSE(WillDraw(impl_layer.get(), DRAW_MODE_RESOURCELESS_SOFTWARE));
  }
}

TEST_F(TextureLayerImplWithResourceTest, TestImplLayerCallbacks) {
  host_impl_.CreatePendingTree();
  std::unique_ptr<TextureLayerImpl> pending_layer;
  pending_layer = TextureLayerImpl::Create(host_impl_.pending_tree(), 1);
  ASSERT_TRUE(pending_layer);

  std::unique_ptr<LayerImpl> active_layer(
      pending_layer->CreateLayerImpl(host_impl_.active_tree()));
  ASSERT_TRUE(active_layer);

  pending_layer->SetTransferableResource(
      test_data_.resource1_,
      viz::SingleReleaseCallback::Create(test_data_.release_callback1_));

  // Test multiple commits without an activation. The resource wasn't used so
  // the original sync token is returned.
  EXPECT_CALL(
      test_data_.mock_callback_,
      Release(test_data_.mailbox_name1_, test_data_.sync_token1_, false))
      .Times(1);
  pending_layer->SetTransferableResource(
      test_data_.resource2_,
      viz::SingleReleaseCallback::Create(test_data_.release_callback2_));
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // Test callback after activation.
  pending_layer->PushPropertiesTo(active_layer.get());
  active_layer->DidBecomeActive();

  EXPECT_CALL(test_data_.mock_callback_, Release(_, _, _)).Times(0);
  pending_layer->SetTransferableResource(
      test_data_.resource1_,
      viz::SingleReleaseCallback::Create(test_data_.release_callback1_));
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name2_, _, false))
      .Times(1);
  pending_layer->PushPropertiesTo(active_layer.get());
  active_layer->DidBecomeActive();
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // Test resetting the mailbox.
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, _, false))
      .Times(1);
  pending_layer->SetTransferableResource(viz::TransferableResource(), nullptr);
  pending_layer->PushPropertiesTo(active_layer.get());
  active_layer->DidBecomeActive();
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // Test destructor. The resource wasn't used so the original sync token is
  // returned.
  EXPECT_CALL(
      test_data_.mock_callback_,
      Release(test_data_.mailbox_name1_, test_data_.sync_token1_, false))
      .Times(1);
  pending_layer->SetTransferableResource(
      test_data_.resource1_,
      viz::SingleReleaseCallback::Create(test_data_.release_callback1_));
}

TEST_F(TextureLayerImplWithResourceTest,
       TestDestructorCallbackOnCreatedResource) {
  std::unique_ptr<TextureLayerImpl> impl_layer = CreateTextureLayer();
  ASSERT_TRUE(impl_layer);

  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, _, false))
      .Times(1);
  impl_layer->SetTransferableResource(
      test_data_.resource1_,
      viz::SingleReleaseCallback::Create(test_data_.release_callback1_));
  impl_layer->DidBecomeActive();
  EXPECT_TRUE(impl_layer->WillDraw(
      DRAW_MODE_HARDWARE, host_impl_.active_tree()->resource_provider()));
  impl_layer->DidDraw(host_impl_.active_tree()->resource_provider());
  impl_layer->SetTransferableResource(viz::TransferableResource(), nullptr);
}

// Checks that TextureLayer::Update does not cause an extra commit when setting
// the texture mailbox.
class TextureLayerNoExtraCommitForMailboxTest
    : public LayerTreeTest,
      public TextureLayerClient {
 public:
  // TextureLayerClient implementation.
  bool PrepareTransferableResource(
      SharedBitmapIdRegistrar* bitmap_registrar,
      viz::TransferableResource* resource,
      std::unique_ptr<viz::SingleReleaseCallback>* release_callback) override {
    if (layer_tree_host()->SourceFrameNumber() == 1) {
      // Once this has been committed, the resource will be released.
      *resource = viz::TransferableResource();
      return true;
    }

    constexpr gfx::Size size(64, 64);
    *resource = viz::TransferableResource::MakeGL(
        MailboxFromChar('1'), GL_LINEAR, GL_TEXTURE_2D,
        SyncTokenFromUInt(0x123), size, false /* is_overlay_candidate */);
    *release_callback = viz::SingleReleaseCallback::Create(base::BindOnce(
        &TextureLayerNoExtraCommitForMailboxTest::ResourceReleased,
        base::Unretained(this)));
    return true;
  }

  void ResourceReleased(const gpu::SyncToken& sync_token, bool lost_resource) {
    EXPECT_TRUE(sync_token.HasData());
    EndTest();
  }

  void SetupTree() override {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(10, 10));
    root->SetIsDrawable(true);

    texture_layer_ = TextureLayer::CreateForMailbox(this);
    texture_layer_->SetBounds(gfx::Size(10, 10));
    texture_layer_->SetIsDrawable(true);
    root->AddChild(texture_layer_);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeTest::SetupTree();
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommitAndDrawFrame() override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1:
        EXPECT_FALSE(proxy()->MainFrameWillHappenForTesting());
        // Invalidate the texture layer to clear the mailbox before
        // ending the test.
        texture_layer_->SetNeedsDisplay();
        break;
      case 2:
        break;
      default:
        NOTREACHED();
        break;
    }
  }

 private:
  scoped_refptr<TextureLayer> texture_layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(TextureLayerNoExtraCommitForMailboxTest);

// Checks that changing a mailbox in the client for a TextureLayer that's
// invisible correctly works and uses the new mailbox as soon as the layer
// becomes visible (and returns the old one).
class TextureLayerChangeInvisibleMailboxTest
    : public LayerTreeTest,
      public TextureLayerClient {
 public:
  TextureLayerChangeInvisibleMailboxTest()
      : resource_changed_(true),
        resource_(MakeResource('1')),
        resource_returned_(0),
        prepare_called_(0),
        commit_count_(0) {}

  // TextureLayerClient implementation.
  bool PrepareTransferableResource(
      SharedBitmapIdRegistrar* bitmap_registrar,
      viz::TransferableResource* resource,
      std::unique_ptr<viz::SingleReleaseCallback>* release_callback) override {
    ++prepare_called_;
    if (!resource_changed_)
      return false;
    *resource = resource_;
    *release_callback = viz::SingleReleaseCallback::Create(base::BindOnce(
        &TextureLayerChangeInvisibleMailboxTest::ResourceReleased,
        base::Unretained(this)));
    return true;
  }

  viz::TransferableResource MakeResource(char name) {
    constexpr gfx::Size size(64, 64);
    return viz::TransferableResource::MakeGL(
        MailboxFromChar(name), GL_LINEAR, GL_TEXTURE_2D,
        SyncTokenFromUInt(static_cast<uint32_t>(name)), size,
        false /* is_overlay_candidate */);
  }

  void ResourceReleased(const gpu::SyncToken& sync_token, bool lost_resource) {
    EXPECT_TRUE(sync_token.HasData());
    ++resource_returned_;
  }

  void SetupTree() override {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(10, 10));
    root->SetIsDrawable(true);

    solid_layer_ = SolidColorLayer::Create();
    solid_layer_->SetBounds(gfx::Size(10, 10));
    solid_layer_->SetIsDrawable(true);
    solid_layer_->SetBackgroundColor(SK_ColorWHITE);
    root->AddChild(solid_layer_);

    parent_layer_ = Layer::Create();
    parent_layer_->SetBounds(gfx::Size(10, 10));
    parent_layer_->SetIsDrawable(true);
    root->AddChild(parent_layer_);

    texture_layer_ = TextureLayer::CreateForMailbox(this);
    texture_layer_->SetBounds(gfx::Size(10, 10));
    texture_layer_->SetIsDrawable(true);
    parent_layer_->AddChild(texture_layer_);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeTest::SetupTree();
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidReceiveCompositorFrameAck() override {
    ++commit_count_;
    switch (commit_count_) {
      case 1:
        // We should have updated the layer, committing the texture.
        EXPECT_EQ(1, prepare_called_);
        // Make layer invisible.
        parent_layer_->SetOpacity(0.f);
        break;
      case 2:
        // Layer shouldn't have been updated.
        EXPECT_EQ(1, prepare_called_);
        // Change the texture.
        resource_ = MakeResource('2');
        resource_changed_ = true;
        texture_layer_->SetNeedsDisplay();
        // Force a change to make sure we draw a frame.
        solid_layer_->SetBackgroundColor(SK_ColorGRAY);
        break;
      case 3:
        // Layer shouldn't have been updated.
        EXPECT_EQ(1, prepare_called_);
        // So the old resource isn't returned yet.
        EXPECT_EQ(0, resource_returned_);
        // Make layer visible again.
        parent_layer_->SetOpacity(0.9f);
        break;
      case 4:
        // Layer should have been updated.
        EXPECT_EQ(2, prepare_called_);
        // So the old resource should have been returned already.
        EXPECT_EQ(1, resource_returned_);
        texture_layer_->ClearClient();
        break;
      case 5:
        EXPECT_EQ(2, resource_returned_);
        EndTest();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

 private:
  scoped_refptr<SolidColorLayer> solid_layer_;
  scoped_refptr<Layer> parent_layer_;
  scoped_refptr<TextureLayer> texture_layer_;

  // Used on the main thread.
  bool resource_changed_;
  viz::TransferableResource resource_;
  int resource_returned_;
  int prepare_called_;
  int commit_count_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(TextureLayerChangeInvisibleMailboxTest);

// Test that TextureLayerImpl::ReleaseResources can be called which releases
// the resource back to TextureLayerClient.
class TextureLayerReleaseResourcesBase
    : public LayerTreeTest,
      public TextureLayerClient {
 public:
  // TextureLayerClient implementation.
  bool PrepareTransferableResource(
      SharedBitmapIdRegistrar* bitmap_registrar,
      viz::TransferableResource* resource,
      std::unique_ptr<viz::SingleReleaseCallback>* release_callback) override {
    constexpr gfx::Size size(64, 64);
    *resource = viz::TransferableResource::MakeGL(
        MailboxFromChar('1'), GL_LINEAR, GL_TEXTURE_2D, SyncTokenFromUInt(1),
        size, false /* is_overlay_candidate */);
    *release_callback = viz::SingleReleaseCallback::Create(
        base::BindOnce(&TextureLayerReleaseResourcesBase::ResourceReleased,
                       base::Unretained(this)));
    return true;
  }

  void ResourceReleased(const gpu::SyncToken& sync_token, bool lost_resource) {
    resource_released_ = true;
  }

  void SetupTree() override {
    LayerTreeTest::SetupTree();

    scoped_refptr<TextureLayer> texture_layer =
        TextureLayer::CreateForMailbox(this);
    texture_layer->SetBounds(gfx::Size(10, 10));
    texture_layer->SetIsDrawable(true);

    layer_tree_host()->root_layer()->AddChild(texture_layer);
    texture_layer_id_ = texture_layer->id();
  }

  void BeginTest() override {
    resource_released_ = false;
    PostSetNeedsCommitToMainThread();
  }

  void DidCommitAndDrawFrame() override { EndTest(); }

  void AfterTest() override { EXPECT_TRUE(resource_released_); }

 protected:
  int texture_layer_id_;

 private:
  bool resource_released_;
};

class TextureLayerReleaseResourcesAfterCommit
    : public TextureLayerReleaseResourcesBase {
 public:
  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    LayerTreeImpl* tree = nullptr;
    tree = host_impl->sync_tree();
    tree->LayerById(texture_layer_id_)->ReleaseResources();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(TextureLayerReleaseResourcesAfterCommit);

class TextureLayerReleaseResourcesAfterActivate
    : public TextureLayerReleaseResourcesBase {
 public:
  void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    host_impl->active_tree()->LayerById(texture_layer_id_)->ReleaseResources();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(TextureLayerReleaseResourcesAfterActivate);

class TextureLayerWithResourceMainThreadDeleted : public LayerTreeTest {
 public:
  void ReleaseCallback(const gpu::SyncToken& sync_token, bool lost_resource) {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());
    EXPECT_FALSE(lost_resource);
    ++callback_count_;
    EndTest();
  }

  void SetMailbox(char mailbox_char) {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());
    std::unique_ptr<viz::SingleReleaseCallback> callback =
        viz::SingleReleaseCallback::Create(base::BindOnce(
            &TextureLayerWithResourceMainThreadDeleted::ReleaseCallback,
            base::Unretained(this)));
    constexpr gfx::Size size(64, 64);
    auto resource = viz::TransferableResource::MakeGL(
        MailboxFromChar(mailbox_char), GL_LINEAR, GL_TEXTURE_2D,
        SyncTokenFromUInt(static_cast<uint32_t>(mailbox_char)), size,
        false /* is_overlay_candidate */);
    layer_->SetTransferableResource(resource, std::move(callback));
  }

  void SetupTree() override {
    gfx::Size bounds(100, 100);
    root_ = Layer::Create();
    root_->SetBounds(bounds);

    layer_ = TextureLayer::CreateForMailbox(nullptr);
    layer_->SetIsDrawable(true);
    layer_->SetBounds(bounds);

    root_->AddChild(layer_);
    layer_tree_host()->SetRootLayer(root_);
    layer_tree_host()->SetViewportRectAndScale(gfx::Rect(bounds), 1.f,
                                               viz::LocalSurfaceIdAllocation());
  }

  void BeginTest() override {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());

    callback_count_ = 0;

    // Set the resource on the main thread.
    SetMailbox('1');
    EXPECT_EQ(0, callback_count_);

    PostSetNeedsCommitToMainThread();
  }

  void DidCommitAndDrawFrame() override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1:
        // Delete the TextureLayer on the main thread while the resource is in
        // the impl tree.
        layer_->RemoveFromParent();
        layer_ = nullptr;
        break;
    }
  }

  void AfterTest() override { EXPECT_EQ(1, callback_count_); }

 private:
  base::ThreadChecker main_thread_;
  int callback_count_;
  scoped_refptr<Layer> root_;
  scoped_refptr<TextureLayer> layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(TextureLayerWithResourceMainThreadDeleted);

class TextureLayerWithResourceImplThreadDeleted : public LayerTreeTest {
 public:
  void ReleaseCallback(const gpu::SyncToken& sync_token, bool lost_resource) {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());
    EXPECT_FALSE(lost_resource);
    ++callback_count_;
    EndTest();
  }

  void SetMailbox(char mailbox_char) {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());
    std::unique_ptr<viz::SingleReleaseCallback> callback =
        viz::SingleReleaseCallback::Create(base::BindOnce(
            &TextureLayerWithResourceImplThreadDeleted::ReleaseCallback,
            base::Unretained(this)));
    constexpr gfx::Size size(64, 64);
    auto resource = viz::TransferableResource::MakeGL(
        MailboxFromChar(mailbox_char), GL_LINEAR, GL_TEXTURE_2D,
        SyncTokenFromUInt(static_cast<uint32_t>(mailbox_char)), size,
        false /* is_overlay_candidate */);
    layer_->SetTransferableResource(resource, std::move(callback));
  }

  void SetupTree() override {
    gfx::Size bounds(100, 100);
    root_ = Layer::Create();
    root_->SetBounds(bounds);

    layer_ = TextureLayer::CreateForMailbox(nullptr);
    layer_->SetIsDrawable(true);
    layer_->SetBounds(bounds);

    root_->AddChild(layer_);
    layer_tree_host()->SetRootLayer(root_);
    layer_tree_host()->SetViewportRectAndScale(gfx::Rect(bounds), 1.f,
                                               viz::LocalSurfaceIdAllocation());
  }

  void BeginTest() override {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());

    callback_count_ = 0;

    // Set the resource on the main thread.
    SetMailbox('1');
    EXPECT_EQ(0, callback_count_);

    PostSetNeedsCommitToMainThread();
  }

  void DidCommitAndDrawFrame() override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1:
        // Remove the TextureLayer on the main thread while the resource is in
        // the impl tree, but don't delete the TextureLayer until after the impl
        // tree side is deleted.
        layer_->RemoveFromParent();
        break;
      case 2:
        layer_ = nullptr;
        break;
    }
  }

  void AfterTest() override { EXPECT_EQ(1, callback_count_); }

 private:
  base::ThreadChecker main_thread_;
  int callback_count_;
  scoped_refptr<Layer> root_;
  scoped_refptr<TextureLayer> layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(TextureLayerWithResourceImplThreadDeleted);

class StubTextureLayerClient : public TextureLayerClient {
 public:
  // TextureLayerClient implementation.
  bool PrepareTransferableResource(
      SharedBitmapIdRegistrar* bitmap_registrar,
      viz::TransferableResource* resource,
      std::unique_ptr<viz::SingleReleaseCallback>* release_callback) override {
    return false;
  }
};

class SoftwareLayerTreeHostClient : public StubLayerTreeHostClient {
 public:
  SoftwareLayerTreeHostClient() = default;
  ~SoftwareLayerTreeHostClient() override = default;

  // Caller responsible for unsetting this and maintaining the host's lifetime.
  void SetLayerTreeHost(LayerTreeHost* host) { host_ = host; }

  // StubLayerTreeHostClient overrides.
  void RequestNewLayerTreeFrameSink() override {
    auto sink = FakeLayerTreeFrameSink::CreateSoftware();
    frame_sink_ = sink.get();
    host_->SetLayerTreeFrameSink(std::move(sink));
  }

  FakeLayerTreeFrameSink* frame_sink() const { return frame_sink_; }

 private:
  FakeLayerTreeFrameSink* frame_sink_ = nullptr;
  LayerTreeHost* host_ = nullptr;
};

class SoftwareTextureLayerTest : public LayerTreeTest {
 protected:
  void SetupTree() override {
    root_ = Layer::Create();
    root_->SetBounds(gfx::Size(10, 10));

    // A drawable layer so that frames always get drawn.
    solid_color_layer_ = SolidColorLayer::Create();
    solid_color_layer_->SetIsDrawable(true);
    solid_color_layer_->SetBackgroundColor(SK_ColorRED);
    solid_color_layer_->SetBounds(gfx::Size(10, 10));
    root_->AddChild(solid_color_layer_);

    texture_layer_ = TextureLayer::CreateForMailbox(&client_);
    texture_layer_->SetIsDrawable(true);
    texture_layer_->SetBounds(gfx::Size(10, 10));
    layer_tree_host()->SetRootLayer(root_);
    LayerTreeTest::SetupTree();
  }

  std::unique_ptr<TestLayerTreeFrameSink> CreateLayerTreeFrameSink(
      const viz::RendererSettings& renderer_settings,
      double refresh_rate,
      scoped_refptr<viz::ContextProvider> compositor_context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider)
      override {
    constexpr bool disable_display_vsync = false;
    bool synchronous_composite =
        !HasImplThread() &&
        !layer_tree_host()->GetSettings().single_thread_proxy_scheduler;
    auto sink = std::make_unique<TestLayerTreeFrameSink>(
        nullptr, nullptr, gpu_memory_buffer_manager(), renderer_settings,
        ImplThreadTaskRunner(), synchronous_composite, disable_display_vsync,
        refresh_rate);
    frame_sink_ = sink.get();
    num_frame_sinks_created_++;
    return sink;
  }

  std::unique_ptr<viz::OutputSurface> CreateDisplayOutputSurfaceOnThread(
      scoped_refptr<viz::ContextProvider> compositor_context_provider)
      override {
    return viz::FakeOutputSurface::CreateSoftware(
        std::make_unique<viz::SoftwareOutputDevice>());
  }

  StubTextureLayerClient client_;
  scoped_refptr<Layer> root_;
  scoped_refptr<SolidColorLayer> solid_color_layer_;
  scoped_refptr<TextureLayer> texture_layer_;
  TestLayerTreeFrameSink* frame_sink_ = nullptr;
  int num_frame_sinks_created_ = 0;
};

class SoftwareTextureLayerSwitchTreesTest : public SoftwareTextureLayerTest {
 protected:
  void BeginTest() override {
    PostSetNeedsCommitToMainThread();

    gfx::Size size(1, 1);
    viz::ResourceFormat format = viz::RGBA_8888;

    id_ = viz::SharedBitmap::GenerateId();
    bitmap_ = base::MakeRefCounted<CrossThreadSharedBitmap>(
        id_, viz::bitmap_allocation::AllocateSharedBitmap(size, format), size,
        format);
  }

  void DidCommitAndDrawFrame() override {
    step_ = layer_tree_host()->SourceFrameNumber();
    switch (step_) {
      case 1:
        // The test starts by inserting the TextureLayer to the tree.
        root_->AddChild(texture_layer_);
        // And registers a SharedBitmapId, which should be given to the
        // LayerTreeFrameSink.
        registration_ = texture_layer_->RegisterSharedBitmapId(id_, bitmap_);
        // Give the TextureLayer a resource so it contributes to the frame. It
        // doesn't need to register the SharedBitmapId otherwise.
        texture_layer_->SetTransferableResource(
            viz::TransferableResource::MakeSoftware(id_, gfx::Size(1, 1),
                                                    viz::RGBA_8888),
            viz::SingleReleaseCallback::Create(
                base::BindOnce([](const gpu::SyncToken&, bool) {})));
        break;
      case 2:
        // When the layer is removed from the tree, the bitmap should be
        // unregistered.
        texture_layer_->RemoveFromParent();
        break;
      case 3:
        // When the layer is added to a new tree, the SharedBitmapId is
        // registered again.
        root_->AddChild(texture_layer_);
        break;
      case 4:
        // If the layer is removed and added back to the same tree in one
        // commit, there should be no side effects, the bitmap stays
        // registered.
        texture_layer_->RemoveFromParent();
        root_->AddChild(texture_layer_);
        break;
      case 5:
        // Release the TransferableResource before shutdown.
        texture_layer_->ClearClient();
        break;
      case 6:
        EndTest();
    }
  }

  void DisplayReceivedCompositorFrameOnThread(
      const viz::CompositorFrame& frame) override {
    switch (step_) {
      case 0:
        // Before commit 1, the |texture_layer_| has no SharedBitmapId yet.
        EXPECT_EQ(0u, frame_sink_->owned_bitmaps().size());
        verified_frames_++;
        break;
      case 1:
        // For commit 1, we added a SharedBitmapId to |texture_layer_|.
        EXPECT_EQ(1u, frame_sink_->owned_bitmaps().size());
        EXPECT_EQ(*frame_sink_->owned_bitmaps().begin(), id_);
        verified_frames_++;
        break;
      case 2:
        // For commit 2, we removed |texture_layer_| from the tree.
        EXPECT_EQ(0u, frame_sink_->owned_bitmaps().size());
        verified_frames_++;
        break;
      case 3:
        // For commit 3, we added |texture_layer_| back to the tree.
        EXPECT_EQ(1u, frame_sink_->owned_bitmaps().size());
        EXPECT_EQ(*frame_sink_->owned_bitmaps().begin(), id_);
        verified_frames_++;
        break;
      case 4:
        // For commit 3, we removed+added |texture_layer_| back to the tree.
        EXPECT_EQ(1u, frame_sink_->owned_bitmaps().size());
        EXPECT_EQ(*frame_sink_->owned_bitmaps().begin(), id_);
        verified_frames_++;
        break;
    }
  }

  void AfterTest() override { EXPECT_EQ(5, verified_frames_); }

  int step_ = 0;
  int verified_frames_ = 0;
  viz::SharedBitmapId id_;
  SharedBitmapIdRegistration registration_;
  scoped_refptr<CrossThreadSharedBitmap> bitmap_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(SoftwareTextureLayerSwitchTreesTest);

// Verify that duplicate SharedBitmapIds aren't registered if resources are
// purged due to memory pressure.
class SoftwareTextureLayerPurgeMemoryTest : public SoftwareTextureLayerTest {
 protected:
  void BeginTest() override {
    PostSetNeedsCommitToMainThread();

    const gfx::Size size(1, 1);
    const viz::ResourceFormat format = viz::RGBA_8888;

    id_ = viz::SharedBitmap::GenerateId();
    bitmap_ = base::MakeRefCounted<CrossThreadSharedBitmap>(
        id_, viz::bitmap_allocation::AllocateSharedBitmap(size, format), size,
        format);
  }

  void DidCommitAndDrawFrame() override {
    step_ = layer_tree_host()->SourceFrameNumber();
    switch (step_) {
      case 1:
        // The test starts by inserting the TextureLayer to the tree.
        root_->AddChild(texture_layer_);
        // And registers a SharedBitmapId, which should be given to the
        // LayerTreeFrameSink.
        registration_ = texture_layer_->RegisterSharedBitmapId(id_, bitmap_);
        // Give the TextureLayer a resource so it contributes to the frame. It
        // doesn't need to register the SharedBitmapId otherwise.
        texture_layer_->SetTransferableResource(
            viz::TransferableResource::MakeSoftware(id_, gfx::Size(1, 1),
                                                    viz::RGBA_8888),
            viz::SingleReleaseCallback::Create(
                base::BindOnce([](const gpu::SyncToken&, bool) {})));
        break;
      case 2:
        // Draw again after OnPurgeMemory() was called on the impl thread so we
        // can verify that duplicate SharedBitmapIds aren't registered by
        // TextureLayerImpl.
        texture_layer_->SetNeedsDisplay();
        break;
      case 3:
        // Release the TransferableResource before shutdown.
        texture_layer_->ClearClient();
        break;
      case 4:
        EndTest();
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    // TextureLayerImpl will have registered the SharedBitmapId at this point.
    // Call OnPurgeMemory() to ensure that the same SharedBitmapId doesn't get
    // registered again on the next draw.
    if (step_ == 1)
      base::MemoryPressureListener::SimulatePressureNotification(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  }

  void DisplayReceivedCompositorFrameOnThread(
      const viz::CompositorFrame& frame) override {
    if (step_ == 0) {
      // Before commit 1, the |texture_layer_| has no SharedBitmapId yet.
      EXPECT_THAT(frame_sink_->owned_bitmaps(), testing::IsEmpty());
      verified_frames_++;
    } else {
      // After commit 1, we added a SharedBitmapId to |texture_layer_|.
      EXPECT_THAT(frame_sink_->owned_bitmaps(), testing::ElementsAre(id_));
      verified_frames_++;
    }
  }

  void AfterTest() override { EXPECT_EQ(4, verified_frames_); }

  int step_ = 0;
  int verified_frames_ = 0;
  viz::SharedBitmapId id_;
  SharedBitmapIdRegistration registration_;
  scoped_refptr<CrossThreadSharedBitmap> bitmap_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(SoftwareTextureLayerPurgeMemoryTest);

class SoftwareTextureLayerMultipleRegisterTest
    : public SoftwareTextureLayerTest {
 protected:
  void BeginTest() override {
    PostSetNeedsCommitToMainThread();

    gfx::Size size(1, 1);
    viz::ResourceFormat format = viz::RGBA_8888;

    id1_ = viz::SharedBitmap::GenerateId();
    bitmap1_ = base::MakeRefCounted<CrossThreadSharedBitmap>(
        id1_, viz::bitmap_allocation::AllocateSharedBitmap(size, format), size,
        format);
    id2_ = viz::SharedBitmap::GenerateId();
    bitmap2_ = base::MakeRefCounted<CrossThreadSharedBitmap>(
        id2_, viz::bitmap_allocation::AllocateSharedBitmap(size, format), size,
        format);
  }

  void DidCommitAndDrawFrame() override {
    step_ = layer_tree_host()->SourceFrameNumber();
    switch (step_) {
      case 1:
        // The test starts by inserting the TextureLayer to the tree.
        root_->AddChild(texture_layer_);
        // And registers 2 SharedBitmapIds, which should be given to the
        // LayerTreeFrameSink.
        registration1_ = texture_layer_->RegisterSharedBitmapId(id1_, bitmap1_);
        registration2_ = texture_layer_->RegisterSharedBitmapId(id2_, bitmap2_);
        // Give the TextureLayer a resource so it contributes to the frame. It
        // doesn't need to register the SharedBitmapId otherwise.
        texture_layer_->SetTransferableResource(
            viz::TransferableResource::MakeSoftware(id1_, gfx::Size(1, 1),
                                                    viz::RGBA_8888),
            viz::SingleReleaseCallback::Create(
                base::BindOnce([](const gpu::SyncToken&, bool) {})));
        break;
      case 2:
        // Drop one registration, and force a commit and SubmitCompositorFrame
        // so that we can see it.
        registration2_ = SharedBitmapIdRegistration();
        texture_layer_->SetNeedsDisplay();
        break;
      case 3:
        // Drop the other registration.
        texture_layer_->ClearClient();
        registration1_ = SharedBitmapIdRegistration();
        break;
      case 4:
        EndTest();
    }
  }

  void DisplayReceivedCompositorFrameOnThread(
      const viz::CompositorFrame& frame) override {
    switch (step_) {
      case 0:
        // Before commit 1, the |texture_layer_| has no SharedBitmapId yet.
        EXPECT_EQ(0u, frame_sink_->owned_bitmaps().size());
        verified_frames_++;
        break;
      case 1:
        // For commit 1, we added 2 SharedBitmapIds to |texture_layer_|.
        EXPECT_EQ(2u, frame_sink_->owned_bitmaps().size());
        verified_frames_++;
        break;
      case 2:
        // For commit 2, we removed one SharedBitmapId.
        EXPECT_EQ(1u, frame_sink_->owned_bitmaps().size());
        EXPECT_EQ(*frame_sink_->owned_bitmaps().begin(), id1_);
        verified_frames_++;
        break;
      case 3:
        // For commit 3, we removed the other SharedBitmapId.
        EXPECT_EQ(0u, frame_sink_->owned_bitmaps().size());
        verified_frames_++;
        break;
    }
  }

  void AfterTest() override { EXPECT_EQ(4, verified_frames_); }

  int step_ = 0;
  int verified_frames_ = 0;
  viz::SharedBitmapId id1_;
  viz::SharedBitmapId id2_;
  SharedBitmapIdRegistration registration1_;
  SharedBitmapIdRegistration registration2_;
  scoped_refptr<CrossThreadSharedBitmap> bitmap1_;
  scoped_refptr<CrossThreadSharedBitmap> bitmap2_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(SoftwareTextureLayerMultipleRegisterTest);

class SoftwareTextureLayerRegisterUnregisterTest
    : public SoftwareTextureLayerTest {
 protected:
  void BeginTest() override {
    PostSetNeedsCommitToMainThread();

    gfx::Size size(1, 1);
    viz::ResourceFormat format = viz::RGBA_8888;

    id1_ = viz::SharedBitmap::GenerateId();
    bitmap1_ = base::MakeRefCounted<CrossThreadSharedBitmap>(
        id1_, viz::bitmap_allocation::AllocateSharedBitmap(size, format), size,
        format);
    id2_ = viz::SharedBitmap::GenerateId();
    bitmap2_ = base::MakeRefCounted<CrossThreadSharedBitmap>(
        id2_, viz::bitmap_allocation::AllocateSharedBitmap(size, format), size,
        format);
  }

  void DidCommitAndDrawFrame() override {
    step_ = layer_tree_host()->SourceFrameNumber();
    switch (step_) {
      case 1:
        // The test starts by inserting the TextureLayer to the tree.
        root_->AddChild(texture_layer_);
        // And registers 2 SharedBitmapIds, which would be given to the
        // LayerTreeFrameSink. But we unregister one.
        {
          registration1_ =
              texture_layer_->RegisterSharedBitmapId(id1_, bitmap1_);
          // We explicitly drop this registration by letting it go out of scope
          // and being destroyed. Versus the registration1_ which we drop by
          // assigning an empty registration to it. Both should do the same
          // thing.
          SharedBitmapIdRegistration temp_reg =
              texture_layer_->RegisterSharedBitmapId(id2_, bitmap2_);
        }
        // Give the TextureLayer a resource so it contributes to the frame. It
        // doesn't need to register the SharedBitmapId otherwise.
        texture_layer_->SetTransferableResource(
            viz::TransferableResource::MakeSoftware(id1_, gfx::Size(1, 1),
                                                    viz::RGBA_8888),
            viz::SingleReleaseCallback::Create(
                base::BindOnce([](const gpu::SyncToken&, bool) {})));
        break;
      case 2:
        // Drop the other registration.
        texture_layer_->ClearClient();
        registration1_ = SharedBitmapIdRegistration();
        break;
      case 3:
        EndTest();
    }
  }

  void DisplayReceivedCompositorFrameOnThread(
      const viz::CompositorFrame& frame) override {
    switch (step_) {
      case 0:
        // Before commit 1, the |texture_layer_| has no SharedBitmapId yet.
        EXPECT_EQ(0u, frame_sink_->owned_bitmaps().size());
        verified_frames_++;
        break;
      case 1:
        // For commit 1, we added 1 SharedBitmapId to |texture_layer_|.
        EXPECT_EQ(1u, frame_sink_->owned_bitmaps().size());
        EXPECT_EQ(*frame_sink_->owned_bitmaps().begin(), id1_);
        verified_frames_++;
        break;
      case 2:
        // For commit 2, we removed the other SharedBitmapId.
        EXPECT_EQ(0u, frame_sink_->owned_bitmaps().size());
        verified_frames_++;
        break;
    }
  }

  void AfterTest() override { EXPECT_EQ(3, verified_frames_); }

  int step_ = 0;
  int verified_frames_ = 0;
  viz::SharedBitmapId id1_;
  viz::SharedBitmapId id2_;
  SharedBitmapIdRegistration registration1_;
  scoped_refptr<CrossThreadSharedBitmap> bitmap1_;
  scoped_refptr<CrossThreadSharedBitmap> bitmap2_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(SoftwareTextureLayerRegisterUnregisterTest);

class SoftwareTextureLayerLoseFrameSinkTest : public SoftwareTextureLayerTest {
 protected:
  void BeginTest() override {
    PostSetNeedsCommitToMainThread();

    gfx::Size size(1, 1);
    viz::ResourceFormat format = viz::RGBA_8888;

    id_ = viz::SharedBitmap::GenerateId();
    bitmap_ = base::MakeRefCounted<CrossThreadSharedBitmap>(
        id_, viz::bitmap_allocation::AllocateSharedBitmap(size, format), size,
        format);
  }

  void DidCommitAndDrawFrame() override {
    // We run the next step in a clean stack, so that we don't cause side
    // effects that will interfere with this current stack unwinding.
    // Specifically, removing the LayerTreeFrameSink destroys the Display
    // and the BeginFrameSource, but they can be on the stack (see
    // https://crbug.com/829484).
    MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&SoftwareTextureLayerLoseFrameSinkTest::NextStep,
                       base::Unretained(this)));
  }

  void NextStep() {
    step_ = layer_tree_host()->SourceFrameNumber();
    switch (step_) {
      case 1:
        // The test starts by inserting the TextureLayer to the tree.
        root_->AddChild(texture_layer_);
        // And registers a SharedBitmapId, which should be given to the
        // LayerTreeFrameSink.
        registration_ = texture_layer_->RegisterSharedBitmapId(id_, bitmap_);
        // Give the TextureLayer a resource so it contributes to the frame. It
        // doesn't need to register the SharedBitmapId otherwise.
        texture_layer_->SetTransferableResource(
            viz::TransferableResource::MakeSoftware(id_, gfx::Size(1, 1),
                                                    viz::RGBA_8888),
            viz::SingleReleaseCallback::Create(base::BindOnce(
                &SoftwareTextureLayerLoseFrameSinkTest::ReleaseCallback,
                base::Unretained(this))));
        EXPECT_FALSE(released_);
        break;
      case 2:
        // The frame sink is lost. The host will make a new one and submit
        // another frame, with the id being registered again.
        layer_tree_host()->SetVisible(false);
        first_frame_sink_ =
            layer_tree_host()->ReleaseLayerTreeFrameSink().get();
        layer_tree_host()->SetVisible(true);
        texture_layer_->SetNeedsDisplay();
        EXPECT_FALSE(released_);
        break;
      case 3:
        // Even though the frame sink was lost, the software resource given to
        // the TextureLayer was not lost/returned.
        EXPECT_FALSE(released_);
        // Release the TransferableResource before shutdown, the test ends when
        // it is released.
        texture_layer_->ClearClient();
    }
  }

  void DisplayReceivedCompositorFrameOnThread(
      const viz::CompositorFrame& frame) override {
    switch (step_) {
      case 0:
        // Before commit 1, the |texture_layer_| has no SharedBitmapId yet.
        EXPECT_EQ(0u, frame_sink_->owned_bitmaps().size());
        verified_frames_++;
        break;
      case 1:
        // For commit 1, we added a SharedBitmapId to |texture_layer_|.
        EXPECT_EQ(1, num_frame_sinks_created_);
        EXPECT_EQ(1u, frame_sink_->owned_bitmaps().size());
        EXPECT_EQ(*frame_sink_->owned_bitmaps().begin(), id_);
        verified_frames_++;
        break;
      case 2:
        // For commit 2, we should still have the SharedBitmapId in the new
        // frame sink.
        EXPECT_EQ(2, num_frame_sinks_created_);
        EXPECT_EQ(1u, frame_sink_->owned_bitmaps().size());
        verified_frames_++;
        break;
    }
  }

  void ReleaseCallback(const gpu::SyncToken& token, bool lost) {
    // The software resource is not released when the LayerTreeFrameSink is lost
    // since software resources are not destroyed by the GPU process dying. It
    // is released only after we call TextureLayer::ClearClient().
    EXPECT_EQ(layer_tree_host()->SourceFrameNumber(), 4);
    released_ = true;
    EndTest();
  }

  void AfterTest() override { EXPECT_EQ(3, verified_frames_); }

  int step_ = 0;
  int verified_frames_ = 0;
  bool released_ = false;
  viz::SharedBitmapId id_;
  SharedBitmapIdRegistration registration_;
  scoped_refptr<CrossThreadSharedBitmap> bitmap_;
  // Keeps a pointer value of the first frame sink, which will be removed
  // from the host and destroyed.
  void* first_frame_sink_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(SoftwareTextureLayerLoseFrameSinkTest);

class SoftwareTextureLayerUnregisterRegisterTest
    : public SoftwareTextureLayerTest {
 protected:
  void BeginTest() override {
    PostSetNeedsCommitToMainThread();

    gfx::Size size(1, 1);
    viz::ResourceFormat format = viz::RGBA_8888;

    id_ = viz::SharedBitmap::GenerateId();
    bitmap_ = base::MakeRefCounted<CrossThreadSharedBitmap>(
        id_, viz::bitmap_allocation::AllocateSharedBitmap(size, format), size,
        format);
  }

  void DidCommitAndDrawFrame() override {
    step_ = layer_tree_host()->SourceFrameNumber();
    switch (step_) {
      case 1:
        // The test starts by inserting the TextureLayer to the tree.
        root_->AddChild(texture_layer_);

        // We do a Register request, Unregister request, and then another
        // Register request. The final register request should stick.
        // And registers 2 SharedBitmapIds, which would be given to the
        // LayerTreeFrameSink. But we unregister one.
        {
          // Register-Unregister-
          SharedBitmapIdRegistration temp_reg =
              texture_layer_->RegisterSharedBitmapId(id_, bitmap_);
        }
        // Register.
        registration_ = texture_layer_->RegisterSharedBitmapId(id_, bitmap_);

        // Give the TextureLayer a resource so it contributes to the frame. It
        // doesn't need to register the SharedBitmapId otherwise.
        texture_layer_->SetTransferableResource(
            viz::TransferableResource::MakeSoftware(id_, gfx::Size(1, 1),
                                                    viz::RGBA_8888),
            viz::SingleReleaseCallback::Create(
                base::BindOnce([](const gpu::SyncToken&, bool) {})));
        break;
      case 2:
        // Release the TransferableResource before shutdown.
        texture_layer_->ClearClient();
        break;
      case 3:
        EndTest();
    }
  }

  void DisplayReceivedCompositorFrameOnThread(
      const viz::CompositorFrame& frame) override {
    switch (step_) {
      case 0:
        // Before commit 1, the |texture_layer_| has no SharedBitmapId yet.
        EXPECT_EQ(0u, frame_sink_->owned_bitmaps().size());
        verified_frames_++;
        break;
      case 1:
        // For commit 1, we added 1 SharedBitmapId to |texture_layer_|.
        EXPECT_EQ(1u, frame_sink_->owned_bitmaps().size());
        EXPECT_EQ(*frame_sink_->owned_bitmaps().begin(), id_);
        verified_frames_++;
        break;
    }
  }

  void AfterTest() override { EXPECT_EQ(2, verified_frames_); }

  int step_ = 0;
  int verified_frames_ = 0;
  viz::SharedBitmapId id_;
  SharedBitmapIdRegistration registration_;
  scoped_refptr<CrossThreadSharedBitmap> bitmap_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(SoftwareTextureLayerUnregisterRegisterTest);

}  // namespace
}  // namespace cc
