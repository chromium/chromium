// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/proxy_impl.h"
#include "cc/trees/proxy_main.h"

namespace cc {

class LayerTreeHostProxyTest : public LayerTreeTest {
 protected:
  void SetupTree() override {
    update_check_layer_ = FakePictureLayer::Create(&client_);
    layer_tree_host()->SetRootLayer(update_check_layer_);
    LayerTreeTest::SetupTree();
    client_.set_bounds(update_check_layer_->bounds());
  }

  FakePictureLayer* update_check_layer() const {
    return update_check_layer_.get();
  }

  ProxyMain* GetProxyMain() {
    DCHECK(HasImplThread());
    return static_cast<ProxyMain*>(proxy());
  }

 private:
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> update_check_layer_;
};

class LayerTreeHostProxyTestSetNeedsCommit : public LayerTreeHostProxyTest {
 protected:
  LayerTreeHostProxyTestSetNeedsCommit() = default;
  LayerTreeHostProxyTestSetNeedsCommit(
      const LayerTreeHostProxyTestSetNeedsCommit&) = delete;
  ~LayerTreeHostProxyTestSetNeedsCommit() override = default;

  LayerTreeHostProxyTestSetNeedsCommit& operator=(
      const LayerTreeHostProxyTestSetNeedsCommit&) = delete;

  void BeginTest() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMain()->max_requested_pipeline_stage());

    proxy()->SetNeedsCommit();

    EXPECT_EQ(ProxyMain::COMMIT_PIPELINE_STAGE,
              GetProxyMain()->max_requested_pipeline_stage());
  }

  void DidBeginMainFrame() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMain()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMain()->current_pipeline_stage());
  }

  void DidCommit() override {
    EXPECT_EQ(1, update_check_layer()->update_count());
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMain()->current_pipeline_stage());
    EndTest();
  }
};

MULTI_THREAD_TEST_F(LayerTreeHostProxyTestSetNeedsCommit);

class LayerTreeHostProxyTestSetNeedsAnimate : public LayerTreeHostProxyTest {
 protected:
  LayerTreeHostProxyTestSetNeedsAnimate() = default;
  LayerTreeHostProxyTestSetNeedsAnimate(
      const LayerTreeHostProxyTestSetNeedsAnimate&) = delete;
  ~LayerTreeHostProxyTestSetNeedsAnimate() override = default;

  LayerTreeHostProxyTestSetNeedsAnimate& operator=(
      const LayerTreeHostProxyTestSetNeedsAnimate&) = delete;

  void BeginTest() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMain()->max_requested_pipeline_stage());

    proxy()->SetNeedsAnimate();

    EXPECT_EQ(ProxyMain::ANIMATE_PIPELINE_STAGE,
              GetProxyMain()->max_requested_pipeline_stage());
  }

  void DidBeginMainFrame() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMain()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMain()->current_pipeline_stage());
    EndTest();
  }
};

MULTI_THREAD_TEST_F(LayerTreeHostProxyTestSetNeedsAnimate);

class LayerTreeHostProxyTestSetNeedsUpdateLayers
    : public LayerTreeHostProxyTest {
 protected:
  LayerTreeHostProxyTestSetNeedsUpdateLayers() = default;
  LayerTreeHostProxyTestSetNeedsUpdateLayers(
      const LayerTreeHostProxyTestSetNeedsUpdateLayers&) = delete;
  ~LayerTreeHostProxyTestSetNeedsUpdateLayers() override = default;

  LayerTreeHostProxyTestSetNeedsUpdateLayers& operator=(
      const LayerTreeHostProxyTestSetNeedsUpdateLayers&) = delete;

  void BeginTest() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMain()->max_requested_pipeline_stage());

    proxy()->SetNeedsUpdateLayers();

    EXPECT_EQ(ProxyMain::UPDATE_LAYERS_PIPELINE_STAGE,
              GetProxyMain()->max_requested_pipeline_stage());
  }

  void DidBeginMainFrame() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMain()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMain()->current_pipeline_stage());
  }

  void DidCommit() override {
    EXPECT_EQ(1, update_check_layer()->update_count());
    EndTest();
  }
};

MULTI_THREAD_TEST_F(LayerTreeHostProxyTestSetNeedsUpdateLayers);

class LayerTreeHostProxyTestSetNeedsUpdateLayersWhileAnimating
    : public LayerTreeHostProxyTest {
 protected:
  LayerTreeHostProxyTestSetNeedsUpdateLayersWhileAnimating() = default;
  LayerTreeHostProxyTestSetNeedsUpdateLayersWhileAnimating(
      const LayerTreeHostProxyTestSetNeedsUpdateLayersWhileAnimating&) = delete;
  ~LayerTreeHostProxyTestSetNeedsUpdateLayersWhileAnimating() override =
      default;

  LayerTreeHostProxyTestSetNeedsUpdateLayersWhileAnimating& operator=(
      const LayerTreeHostProxyTestSetNeedsUpdateLayersWhileAnimating&) = delete;

  void BeginTest() override {}

  void WillBeginMainFrame() override {
    if (layer_tree_host()->SourceFrameNumber() != 1)
      return;

    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMain()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::ANIMATE_PIPELINE_STAGE,
              GetProxyMain()->current_pipeline_stage());
    EXPECT_EQ(ProxyMain::ANIMATE_PIPELINE_STAGE,
              GetProxyMain()->final_pipeline_stage());

    proxy()->SetNeedsUpdateLayers();

    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMain()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::UPDATE_LAYERS_PIPELINE_STAGE,
              GetProxyMain()->final_pipeline_stage());
  }

  void DidBeginMainFrame() override {
    if (layer_tree_host()->SourceFrameNumber() != 2)
      return;

    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMain()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMain()->current_pipeline_stage());
  }

  void DidCommit() override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1:
        EXPECT_EQ(1, update_check_layer()->update_count());

        // Wait until the first frame is committed and we enter the desired
        // state to start the test.
        proxy()->SetNeedsAnimate();
        break;
      case 2:
        EXPECT_EQ(2, update_check_layer()->update_count());
        EndTest();
        break;
    }
  }
};

MULTI_THREAD_TEST_F(LayerTreeHostProxyTestSetNeedsUpdateLayersWhileAnimating);

class LayerTreeHostProxyTestSetNeedsCommitWhileAnimating
    : public LayerTreeHostProxyTest {
 protected:
  LayerTreeHostProxyTestSetNeedsCommitWhileAnimating() = default;
  LayerTreeHostProxyTestSetNeedsCommitWhileAnimating(
      const LayerTreeHostProxyTestSetNeedsCommitWhileAnimating&) = delete;
  ~LayerTreeHostProxyTestSetNeedsCommitWhileAnimating() override = default;

  LayerTreeHostProxyTestSetNeedsCommitWhileAnimating& operator=(
      const LayerTreeHostProxyTestSetNeedsCommitWhileAnimating&) = delete;

  void BeginTest() override {}

  void WillBeginMainFrame() override {
    if (layer_tree_host()->SourceFrameNumber() != 1)
      return;

    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMain()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::ANIMATE_PIPELINE_STAGE,
              GetProxyMain()->current_pipeline_stage());
    EXPECT_EQ(ProxyMain::ANIMATE_PIPELINE_STAGE,
              GetProxyMain()->final_pipeline_stage());

    proxy()->SetNeedsCommit();

    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMain()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::COMMIT_PIPELINE_STAGE,
              GetProxyMain()->final_pipeline_stage());
  }

  void DidBeginMainFrame() override {
    if (layer_tree_host()->SourceFrameNumber() != 2)
      return;

    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMain()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMain()->current_pipeline_stage());
  }

  void DidCommit() override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1:
        EXPECT_EQ(1, update_check_layer()->update_count());

        // Wait until the first frame is committed and we enter the desired
        // state to start the test.
        proxy()->SetNeedsAnimate();
        break;
      case 2:
        EXPECT_EQ(2, update_check_layer()->update_count());
        EndTest();
        break;
    }
  }
};

MULTI_THREAD_TEST_F(LayerTreeHostProxyTestSetNeedsCommitWhileAnimating);

class LayerTreeHostProxyTestCommitWaitsForActivation
    : public LayerTreeHostProxyTest {
 protected:
  LayerTreeHostProxyTestCommitWaitsForActivation() = default;
  LayerTreeHostProxyTestCommitWaitsForActivation(
      const LayerTreeHostProxyTestCommitWaitsForActivation&) = delete;

  LayerTreeHostProxyTestCommitWaitsForActivation& operator=(
      const LayerTreeHostProxyTestCommitWaitsForActivation&) = delete;

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void BeginCommitOnThread(LayerTreeHostImpl* impl) override {
    if (impl->sync_tree()->source_frame_number() < 0)
      return;  // The initial commit, don't do anything here.

    // The main thread will request a commit, and may request that it does
    // not complete before activating. So make activation take a long time, to
    // verify that we waited.
    impl->BlockNotifyReadyToActivateForTesting(true);
    {
      base::AutoLock hold(activate_blocked_lock_);
      activate_blocked_ = true;
    }
    switch (impl->sync_tree()->source_frame_number()) {
      case 0: {
        // This is for case 1 in DidCommit.
        auto unblock = base::BindOnce(
            &LayerTreeHostProxyTestCommitWaitsForActivation::UnblockActivation,
            base::Unretained(this), impl);
        ImplThreadTaskRunner()->PostDelayedTask(
            FROM_HERE, std::move(unblock),
            // Use a delay to allow the main frame to start if it would. This
            // should cause failures (or flakiness) if we fail to wait for the
            // activation before starting the main frame.
            base::TimeDelta::FromMilliseconds(16 * 4));
        break;
      }
      case 1:
        // This is for case 2 in DidCommit.
        // Here we don't ever unblock activation. Since the commit hasn't
        // requested to wait, we can verify that activation is blocked when the
        // commit completes (case 3 in DidCommit).
        break;
    }
  }

  void DidCommit() override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1:
        // Request a new commit, but DidCommit will be delayed until activation
        // completes.
        layer_tree_host()->SetNextCommitWaitsForActivation();
        layer_tree_host()->SetNeedsCommit();
        break;
      case 2: {
        base::AutoLock hold(activate_blocked_lock_);
        EXPECT_FALSE(activate_blocked_);
      }
        // Request a new commit, but DidCommit will not be delayed.
        layer_tree_host()->SetNeedsCommit();
        break;
      case 3: {
        base::AutoLock hold(activate_blocked_lock_);
        EXPECT_TRUE(activate_blocked_);
      }
        // This commit completed before unblocking activation.
        EndTest();
        break;
    }
  }

  void UnblockActivation(LayerTreeHostImpl* impl) {
    {
      base::AutoLock hold(activate_blocked_lock_);
      activate_blocked_ = false;
    }
    impl->BlockNotifyReadyToActivateForTesting(false);
  }

 private:
  base::Lock activate_blocked_lock_;
  bool activate_blocked_ = false;
};

MULTI_THREAD_TEST_F(LayerTreeHostProxyTestCommitWaitsForActivation);

// Test for a corner case of main frame before activation (MFBA) and commit
// waits for activation. If a commit (with wait for activation flag set)
// is ready before the activation for a previous commit then the activation
// should not signal the completion event of the second commit.
class LayerTreeHostProxyTestCommitWaitsForActivationMFBA
    : public LayerTreeHostProxyTest {
 protected:
  LayerTreeHostProxyTestCommitWaitsForActivationMFBA() = default;
  LayerTreeHostProxyTestCommitWaitsForActivationMFBA(
      const LayerTreeHostProxyTestCommitWaitsForActivationMFBA&) = delete;

  LayerTreeHostProxyTestCommitWaitsForActivationMFBA& operator=(
      const LayerTreeHostProxyTestCommitWaitsForActivationMFBA&) = delete;

  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->main_frame_before_activation_enabled = true;
    LayerTreeHostProxyTest::InitializeSettings(settings);
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void ReadyToCommitOnThread(LayerTreeHostImpl* impl) override {
    LayerTreeImpl* sync_tree =
        impl->pending_tree() ? impl->pending_tree() : impl->active_tree();
    switch (sync_tree->source_frame_number()) {
      case -1:
        // Block the activation of the initial commit until the second main
        // frame is ready.
        impl->BlockNotifyReadyToActivateForTesting(true);
        break;
      case 0: {
        // This is the main frame with SetNextCommitWaitsForActivation().
        // Activation is currently blocked for the previous main frame (from the
        // case above). We unblock activate to allow this main frame to commit.
        auto unblock = base::BindOnce(
            &LayerTreeHostImpl::BlockNotifyReadyToActivateForTesting,
            base::Unretained(impl), false);
        // Post the unblock instead of doing it immediately so that the main
        // frame is fully processed by the compositor thread, and it has a full
        // opportunity to wrongly unblock the main thread.
        ImplThreadTaskRunner()->PostTask(FROM_HERE, std::move(unblock));
        // Once activation completes, we'll begin the commit for frame 1.
        break;
      }
    }
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    if (impl->active_tree()->source_frame_number() == 0) {
      // The main thread requests a commit does not complete before activating.
      // So make activation take a long time, to verify that we waited.
      impl->BlockNotifyReadyToActivateForTesting(true);
      {
        base::AutoLock hold(activate_blocked_lock_);
        // Record that we've blocked activation for this frame of interest.
        activate_blocked_ = true;
      }
      // Then unblock activation eventually to complete the test. We use a
      // delay to allow the main frame to start if it would. This should cause
      // failures (or flakiness) if we fail to wait for the activation before
      // starting the main frame.
      auto unblock =
          base::BindOnce(&LayerTreeHostProxyTestCommitWaitsForActivationMFBA::
                             UnblockActivation,
                         base::Unretained(this), impl);
      ImplThreadTaskRunner()->PostDelayedTask(
          FROM_HERE, std::move(unblock),
          base::TimeDelta::FromMilliseconds(16 * 4));
    }
  }

  void DidCommit() override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1:
        // Request a new commit, but DidCommit will be delayed until activation
        // completes.
        layer_tree_host()->SetNextCommitWaitsForActivation();
        layer_tree_host()->SetNeedsCommit();
        break;
      case 2:
        // This DidCommit should not happen until activation is done for the
        // frame.
        {
          base::AutoLock hold(activate_blocked_lock_);
          EXPECT_FALSE(activate_blocked_);
        }
        EndTest();
        break;
    }
  }

  void UnblockActivation(LayerTreeHostImpl* impl) {
    {
      base::AutoLock hold(activate_blocked_lock_);
      activate_blocked_ = false;
    }
    impl->BlockNotifyReadyToActivateForTesting(false);
  }

 private:
  base::Lock activate_blocked_lock_;
  bool activate_blocked_ = false;
};

MULTI_THREAD_TEST_F(LayerTreeHostProxyTestCommitWaitsForActivationMFBA);

// Tests that SingleThreadProxy correctly reports pending animations when
// requested from the impl-side.
class LayerTreeHostProxyTestImplFrameCausesAnimatePending
    : public LayerTreeHostProxyTest {
 protected:
  LayerTreeHostProxyTestImplFrameCausesAnimatePending() = default;
  LayerTreeHostProxyTestImplFrameCausesAnimatePending(
      const LayerTreeHostProxyTestImplFrameCausesAnimatePending&) = delete;
  LayerTreeHostProxyTestImplFrameCausesAnimatePending& operator=(
      const LayerTreeHostProxyTestImplFrameCausesAnimatePending&) = delete;

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    switch (host_impl->sync_tree()->source_frame_number()) {
      case 0: {
        EXPECT_FALSE(proxy()->RequestedAnimatePending());
        host_impl->SetNeedsOneBeginImplFrame();
        EXPECT_TRUE(proxy()->RequestedAnimatePending());
        PostSetNeedsCommitToMainThread();
        break;
      }
      case 1: {
        EXPECT_FALSE(proxy()->RequestedAnimatePending());
        EndTest();
        break;
      }
      default: { NOTREACHED(); }
    }
  }
};

SINGLE_THREAD_TEST_F(LayerTreeHostProxyTestImplFrameCausesAnimatePending);

// Test that the SingleThreadProxy correctly records and clears commit requests
// from the impl-side.
class LayerTreeHostProxyTestNeedsCommitFromImpl
    : public LayerTreeHostProxyTest {
 protected:
  LayerTreeHostProxyTestNeedsCommitFromImpl() = default;
  LayerTreeHostProxyTestNeedsCommitFromImpl(
      const LayerTreeHostProxyTestNeedsCommitFromImpl&) = delete;
  LayerTreeHostProxyTestNeedsCommitFromImpl& operator=(
      const LayerTreeHostProxyTestNeedsCommitFromImpl&) = delete;

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    switch (host_impl->sync_tree()->source_frame_number()) {
      case 0: {
        host_impl->SetNeedsCommit();
        MainThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(&LayerTreeHostProxyTestNeedsCommitFromImpl::
                               CheckCommitRequested,
                           base::Unretained(this)));
        break;
      }
      case 1: {
        MainThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(&LayerTreeHostProxyTestNeedsCommitFromImpl::
                               CheckRequestClearedAndEnd,
                           base::Unretained(this)));
        break;
      }
      default: { NOTREACHED(); }
    }
  }

  void CheckCommitRequested() { EXPECT_TRUE(proxy()->CommitRequested()); }

  void CheckRequestClearedAndEnd() {
    EXPECT_FALSE(proxy()->CommitRequested());
    EndTest();
  }
};

SINGLE_THREAD_TEST_F(LayerTreeHostProxyTestNeedsCommitFromImpl);

// Test that a commit is correctly delayed but is not lost when turning
// invisible, and after turning visible, the commit is executed.
// This is a regression test for https://crbug.com/890008
class LayerTreeHostProxyTestDelayedCommitDueToVisibility
    : public LayerTreeHostProxyTest {
 protected:
  LayerTreeHostProxyTestDelayedCommitDueToVisibility() = default;
  LayerTreeHostProxyTestDelayedCommitDueToVisibility(
      const LayerTreeHostProxyTestDelayedCommitDueToVisibility&) = delete;
  ~LayerTreeHostProxyTestDelayedCommitDueToVisibility() override = default;

  LayerTreeHostProxyTestDelayedCommitDueToVisibility& operator=(
      const LayerTreeHostProxyTestDelayedCommitDueToVisibility&) = delete;

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void WillSendBeginMainFrameOnThread(LayerTreeHostImpl*) override {
    if (!set_invisible_once_) {
      set_invisible_once_ = true;
      PostSetVisibleToMainThread(false);
    }
  }

  void BeginMainFrameAbortedOnThread(LayerTreeHostImpl*,
                                     CommitEarlyOutReason reason) override {
    EXPECT_EQ(CommitEarlyOutReason::ABORTED_NOT_VISIBLE, reason);
    PostSetVisibleToMainThread(true);
  }

  void DidCommit() override { EndTest(); }

 private:
  bool set_invisible_once_ = false;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostProxyTestDelayedCommitDueToVisibility);

}  // namespace cc
