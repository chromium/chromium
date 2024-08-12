// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/base/math_util.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/solid_color_scrollbar_layer.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/cc_test_suite.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/stub_layer_tree_host_single_thread_client.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/transform_node.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Mock;
using ::testing::StrictMock;

#define EXPECT_CALL_MOCK_DELEGATE(obj, call) \
  EXPECT_CALL((obj).mock_delegate(), call)

#define EXPECT_SET_NEEDS_UPDATE(expect, code_to_test)                    \
  do {                                                                   \
    EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsUpdateLayers()) \
        .Times((expect));                                                \
    code_to_test;                                                        \
    layer_tree_host_->VerifyAndClearExpectations();                      \
  } while (false)

#define EXPECT_SET_NEEDS_FULL_TREE_SYNC(expect, code_to_test)            \
  do {                                                                   \
    EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsFullTreeSync()) \
        .Times((expect));                                                \
    code_to_test;                                                        \
    layer_tree_host_->VerifyAndClearExpectations();                      \
  } while (false)

#define EXECUTE_AND_VERIFY_SUBTREE_CHANGED(code_to_test)                      \
  code_to_test;                                                               \
  root->layer_tree_host()->BuildPropertyTreesForTesting();                    \
  EXPECT_FALSE(root->subtree_property_changed());                             \
  EXPECT_TRUE(top->subtree_property_changed());                               \
  EXPECT_TRUE(                                                                \
      base::Contains(const_cast<const LayerTreeHost*>(top->layer_tree_host()) \
                         ->pending_commit_state()                             \
                         ->layers_that_should_push_properties,                \
                     top.get()));                                             \
  EXPECT_TRUE(child->subtree_property_changed());                             \
  EXPECT_TRUE(base::Contains(                                                 \
      const_cast<const LayerTreeHost*>(child->layer_tree_host())              \
          ->pending_commit_state()                                            \
          ->layers_that_should_push_properties,                               \
      child.get()));                                                          \
  EXPECT_TRUE(grand_child->subtree_property_changed());                       \
  EXPECT_TRUE(base::Contains(                                                 \
      const_cast<const LayerTreeHost*>(grand_child->layer_tree_host())        \
          ->pending_commit_state()                                            \
          ->layers_that_should_push_properties,                               \
      grand_child.get()));

#define EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(code_to_test) \
  code_to_test;                                                \
  EXPECT_FALSE(root->subtree_property_changed());              \
  EXPECT_FALSE(top->subtree_property_changed());               \
  EXPECT_FALSE(child->subtree_property_changed());             \
  EXPECT_FALSE(grand_child->subtree_property_changed());

#define EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(code_to_test)           \
  do {                                                             \
    EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit()) \
        .Times(AtLeast(1));                                        \
    code_to_test;                                                  \
    layer_tree_host_->VerifyAndClearExpectations();                \
  } while (false)

#define EXPECT_SET_NEEDS_COMMIT_WAS_NOT_CALLED(code_to_test)                 \
  do {                                                                       \
    EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit()).Times(0); \
    code_to_test;                                                            \
    layer_tree_host_->VerifyAndClearExpectations();                          \
  } while (false)

namespace cc {

namespace {

static auto kArbitrarySourceId1 =
    base::UnguessableToken::CreateForTesting(0xdead, 0xbeef);
static auto kArbitrarySourceId2 =
    base::UnguessableToken::CreateForTesting(0xdead, 0xbee0);

// http://google.github.io/googletest/gmock_for_dummies.html#using-mocks-in-tests
// says that it is undefined behavior if we alternate between calls to
// EXPECT_CALL_MOCK_DELEGATE() and calls to the mock functions. It is also
// undefined behavior if we set new expectations after a call to
// VerifyAndClearExpectations. So we need a way to make mocking expectations
// resettable. This delegate object could help achieve that. When we want to
// reset the expectations, we can delete and recreate the delegate object.
class MockLayerTreeHostDelegate {
 public:
  MOCK_METHOD(void, SetNeedsUpdateLayers, (), ());
  MOCK_METHOD(void, SetNeedsFullTreeSync, (), ());
  MOCK_METHOD(void, SetNeedsCommit, (), ());
};

class FakeLayerTreeHost : public LayerTreeHost {
 public:
  FakeLayerTreeHost(LayerTreeHostSingleThreadClient* single_thread_client,
                    LayerTreeHost::InitParams params)
      : LayerTreeHost(std::move(params), CompositorMode::SINGLE_THREADED),
        mock_delegate_(
            std::make_unique<StrictMock<MockLayerTreeHostDelegate>>()) {
    InitializeSingleThreaded(single_thread_client,
                             base::SingleThreadTaskRunner::GetCurrentDefault());
  }

  CommitState* GetPendingCommitState() { return pending_commit_state(); }
  ThreadUnsafeCommitState& GetThreadUnsafeCommitState() {
    return thread_unsafe_commit_state();
  }

  void SetNeedsUpdateLayers() override {
    mock_delegate_->SetNeedsUpdateLayers();
  }
  void SetNeedsFullTreeSync() override {
    mock_delegate_->SetNeedsFullTreeSync();
  }

  void SetNeedsCommit() override { mock_delegate_->SetNeedsCommit(); }

  StrictMock<MockLayerTreeHostDelegate>& mock_delegate() {
    return *mock_delegate_;
  }

  void VerifyAndClearExpectations() {
    mock_delegate_.reset();
    mock_delegate_ = std::make_unique<StrictMock<MockLayerTreeHostDelegate>>();
  }

 private:
  std::unique_ptr<StrictMock<MockLayerTreeHostDelegate>> mock_delegate_;
};

bool LayerNeedsDisplay(Layer* layer) {
  return !layer->update_rect().IsEmpty();
}

class LayerTest : public testing::Test {
 public:
  LayerTest()
      : host_impl_(LayerTreeSettings(),
                   &task_runner_provider_,
                   &task_graph_runner_) {
    timeline_impl_ =
        AnimationTimeline::Create(AnimationIdProvider::NextTimelineId(),
                                  /* is_impl_only */ true);
    host_impl_.animation_host()->AddAnimationTimeline(timeline_impl_);
  }

  const LayerTreeSettings& settings() { return settings_; }
  scoped_refptr<AnimationTimeline> timeline_impl() { return timeline_impl_; }

 protected:
  void SetUp() override {
    animation_host_ = AnimationHost::CreateForTesting(ThreadInstance::kMain);

    LayerTreeHost::InitParams params;
    params.client = &fake_client_;
    params.settings = &settings_;
    params.task_graph_runner = &task_graph_runner_;
    params.mutator_host = animation_host_.get();

    layer_tree_host_ = std::make_unique<FakeLayerTreeHost>(
        &single_thread_client_, std::move(params));
  }

  void TearDown() override {
    layer_tree_host_->VerifyAndClearExpectations();
    EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsFullTreeSync())
        .Times(AnyNumber());
    parent_ = nullptr;
    child1_ = nullptr;
    child2_ = nullptr;
    child3_ = nullptr;
    grand_child1_ = nullptr;
    grand_child2_ = nullptr;
    grand_child3_ = nullptr;

    layer_tree_host_->SetRootLayer(nullptr);
    animation_host_->SetMutatorHostClient(nullptr);
    layer_tree_host_ = nullptr;
    animation_host_ = nullptr;
  }

  void SimulateCommitForLayer(Layer* layer) {
    layer->PushPropertiesTo(
        layer->CreateLayerImpl(host_impl_.active_tree()).get(),
        *layer_tree_host_->GetPendingCommitState(),
        layer_tree_host_->GetThreadUnsafeCommitState());
  }

  void CommitAndPushProperties(Layer* layer, LayerImpl* layer_impl) {
    auto& unsafe_state = layer_tree_host_->GetThreadUnsafeCommitState();
    std::unique_ptr<CommitState> commit_state = layer_tree_host_->WillCommit(
        /*completion=*/nullptr, /*has_updates=*/true);
    layer->PushPropertiesTo(layer_impl, *commit_state, unsafe_state);
    layer_tree_host_->CommitComplete(
        commit_state->source_frame_number,
        {base::TimeTicks(), base::TimeTicks::Now()});
  }

  void VerifyTestTreeInitialState() const {
    ASSERT_EQ(3U, parent_->children().size());
    EXPECT_EQ(child1_, parent_->children()[0]);
    EXPECT_EQ(child2_, parent_->children()[1]);
    EXPECT_EQ(child3_, parent_->children()[2]);
    EXPECT_EQ(parent_.get(), child1_->parent());
    EXPECT_EQ(parent_.get(), child2_->parent());
    EXPECT_EQ(parent_.get(), child3_->parent());

    ASSERT_EQ(2U, child1_->children().size());
    EXPECT_EQ(grand_child1_, child1_->children()[0]);
    EXPECT_EQ(grand_child2_, child1_->children()[1]);
    EXPECT_EQ(child1_.get(), grand_child1_->parent());
    EXPECT_EQ(child1_.get(), grand_child2_->parent());

    ASSERT_EQ(1U, child2_->children().size());
    EXPECT_EQ(grand_child3_, child2_->children()[0]);
    EXPECT_EQ(child2_.get(), grand_child3_->parent());

    ASSERT_EQ(0U, child3_->children().size());
  }

  void CreateSimpleTestTree() {
    parent_ = Layer::Create();
    child1_ = Layer::Create();
    child2_ = Layer::Create();
    child3_ = Layer::Create();
    grand_child1_ = Layer::Create();
    grand_child2_ = Layer::Create();
    grand_child3_ = Layer::Create();

    EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsFullTreeSync())
        .Times(AnyNumber());
    layer_tree_host_->SetRootLayer(parent_);

    parent_->AddChild(child1_);
    parent_->AddChild(child2_);
    parent_->AddChild(child3_);
    child1_->AddChild(grand_child1_);
    child1_->AddChild(grand_child2_);
    child2_->AddChild(grand_child3_);

    layer_tree_host_->VerifyAndClearExpectations();

    VerifyTestTreeInitialState();
  }

  FakeImplTaskRunnerProvider task_runner_provider_;
  TestTaskGraphRunner task_graph_runner_;
  FakeLayerTreeHostImpl host_impl_;

  StubLayerTreeHostSingleThreadClient single_thread_client_;
  FakeLayerTreeHostClient fake_client_;
  std::unique_ptr<FakeLayerTreeHost> layer_tree_host_;
  std::unique_ptr<AnimationHost> animation_host_;
  scoped_refptr<Layer> parent_;
  scoped_refptr<Layer> child1_;
  scoped_refptr<Layer> child2_;
  scoped_refptr<Layer> child3_;
  scoped_refptr<Layer> grand_child1_;
  scoped_refptr<Layer> grand_child2_;
  scoped_refptr<Layer> grand_child3_;

  scoped_refptr<AnimationTimeline> timeline_impl_;

  LayerTreeSettings settings_;
};

class LayerTestWithLayerList : public LayerTest {
  void SetUp() override {
    settings_.use_layer_lists = true;
    LayerTest::SetUp();
  }
};

TEST_F(LayerTest, BasicCreateAndDestroy) {
  scoped_refptr<Layer> test_layer = Layer::Create();
  ASSERT_TRUE(test_layer.get());

  test_layer->SetLayerTreeHost(layer_tree_host_.get());
  test_layer->SetLayerTreeHost(nullptr);
}

TEST_F(LayerTest, LayerPropertyChangedForSubtree) {
  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsFullTreeSync())
      .Times(AtLeast(1));
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> top = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<Layer> child2 = Layer::Create();
  scoped_refptr<Layer> grand_child = Layer::Create();
  FakeContentLayerClient client;
  scoped_refptr<PictureLayer> mask_layer1 = PictureLayer::Create(&client);
  mask_layer1->SetElementId(LayerIdToElementIdForTesting(mask_layer1->id()));

  layer_tree_host_->SetRootLayer(root);
  root->AddChild(top);
  top->AddChild(child);
  top->AddChild(child2);
  child->AddChild(grand_child);

  // To force a transform node for |top|.
  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit())
      .Times(AtLeast(1));
  gfx::Transform top_transform;
  top_transform.Scale3d(1, 2, 3);
  top->SetTransform(top_transform);
  child->SetForceRenderSurfaceForTesting(true);
  layer_tree_host_->VerifyAndClearExpectations();

  // Resizing without a mask layer or masks_to_bounds, should only require a
  // regular commit. Note that a layer and its mask should match sizes, but
  // the mask isn't in the tree yet, so won't need its own commit.
  gfx::Size arbitrary_size = gfx::Size(1, 2);
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(top->SetBounds(arbitrary_size));
  EXPECT_SET_NEEDS_COMMIT_WAS_NOT_CALLED(
      mask_layer1->SetBounds(arbitrary_size));
  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsFullTreeSync());
  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit())
      .Times(AtLeast(1));
  auto commit_state = layer_tree_host_->WillCommit(/*completion=*/nullptr,
                                                   /*has_updates=*/true);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(top->SetMaskLayer(mask_layer1));
  layer_tree_host_->CommitComplete(commit_state->source_frame_number,
                                   {base::TimeTicks(), base::TimeTicks::Now()});
  layer_tree_host_->VerifyAndClearExpectations();

  // Set up the impl layers after the full tree is constructed, including the
  // mask layer.
  SkBlendMode arbitrary_blend_mode = SkBlendMode::kMultiply;
  std::unique_ptr<LayerImpl> top_impl =
      LayerImpl::Create(host_impl_.active_tree(), top->id());
  std::unique_ptr<LayerImpl> child_impl =
      LayerImpl::Create(host_impl_.active_tree(), child->id());
  std::unique_ptr<LayerImpl> child2_impl =
      LayerImpl::Create(host_impl_.active_tree(), child2->id());
  std::unique_ptr<LayerImpl> grand_child_impl =
      LayerImpl::Create(host_impl_.active_tree(), grand_child->id());
  std::unique_ptr<LayerImpl> mask_layer1_impl =
      mask_layer1->CreateLayerImpl(host_impl_.active_tree());

  host_impl_.active_tree()->set_source_frame_number(
      host_impl_.active_tree()->source_frame_number() + 1);

  auto& unsafe_state = layer_tree_host_->GetThreadUnsafeCommitState();
  layer_tree_host_->WillCommit(
      /*completion=*/nullptr, /*has_updates=*/true);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      top->PushPropertiesTo(top_impl.get(), *commit_state, unsafe_state);
      child->PushPropertiesTo(child_impl.get(), *commit_state, unsafe_state);
      child2->PushPropertiesTo(child2_impl.get(), *commit_state, unsafe_state);
      grand_child->PushPropertiesTo(grand_child_impl.get(), *commit_state,
                                    unsafe_state);
      mask_layer1->PushPropertiesTo(mask_layer1_impl.get(), *commit_state,
                                    unsafe_state););
  layer_tree_host_->CommitComplete(commit_state->source_frame_number,
                                   {base::TimeTicks(), base::TimeTicks::Now()});

  // Once there is a mask layer, resizes require subtree properties to update.
  arbitrary_size = gfx::Size(11, 22);
  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit()).Times(2);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(top->SetBounds(arbitrary_size));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(mask_layer1->SetBounds(arbitrary_size));
  layer_tree_host_->VerifyAndClearExpectations();

  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit()).Times(1);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(top->SetMasksToBounds(true));
  layer_tree_host_->VerifyAndClearExpectations();

  commit_state = layer_tree_host_->WillCommit(/*completion=*/nullptr,
                                              /*has_updates=*/true);

  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      top->PushPropertiesTo(top_impl.get(), *commit_state, unsafe_state);
      child->PushPropertiesTo(child_impl.get(), *commit_state, unsafe_state);
      child2->PushPropertiesTo(child2_impl.get(), *commit_state, unsafe_state);
      grand_child->PushPropertiesTo(grand_child_impl.get(), *commit_state,
                                    unsafe_state));
  layer_tree_host_->CommitComplete(commit_state->source_frame_number,
                                   {base::TimeTicks(), base::TimeTicks::Now()});

  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit()).Times(1);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(top->SetContentsOpaque(true));
  layer_tree_host_->VerifyAndClearExpectations();

  commit_state = layer_tree_host_->WillCommit(/*completion=*/nullptr,
                                              /*has_updates=*/true);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      top->PushPropertiesTo(top_impl.get(), *commit_state, unsafe_state);
      child->PushPropertiesTo(child_impl.get(), *commit_state, unsafe_state);
      child2->PushPropertiesTo(child2_impl.get(), *commit_state, unsafe_state);
      grand_child->PushPropertiesTo(grand_child_impl.get(), *commit_state,
                                    unsafe_state));
  layer_tree_host_->CommitComplete(commit_state->source_frame_number,
                                   {base::TimeTicks(), base::TimeTicks::Now()});

  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit()).Times(1);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(top->SetTrilinearFiltering(true));
  layer_tree_host_->VerifyAndClearExpectations();

  commit_state = layer_tree_host_->WillCommit(/*completion=*/nullptr,
                                              /*has_updates=*/true);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      top->PushPropertiesTo(top_impl.get(), *commit_state, unsafe_state);
      child->PushPropertiesTo(child_impl.get(), *commit_state, unsafe_state);
      child2->PushPropertiesTo(child2_impl.get(), *commit_state, unsafe_state);
      grand_child->PushPropertiesTo(grand_child_impl.get(), *commit_state,
                                    unsafe_state));
  layer_tree_host_->CommitComplete(commit_state->source_frame_number,
                                   {base::TimeTicks(), base::TimeTicks::Now()});

  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit()).Times(1);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(top->SetTrilinearFiltering(false));
  layer_tree_host_->VerifyAndClearExpectations();

  commit_state = layer_tree_host_->WillCommit(/*completion=*/nullptr,
                                              /*has_updates=*/true);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      top->PushPropertiesTo(top_impl.get(), *commit_state, unsafe_state);
      child->PushPropertiesTo(child_impl.get(), *commit_state, unsafe_state);
      child2->PushPropertiesTo(child2_impl.get(), *commit_state, unsafe_state);
      grand_child->PushPropertiesTo(grand_child_impl.get(), *commit_state,
                                    unsafe_state));
  layer_tree_host_->CommitComplete(commit_state->source_frame_number,
                                   {base::TimeTicks(), base::TimeTicks::Now()});

  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit()).Times(2);
  top->SetRoundedCorner({1, 2, 3, 4});
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(top->SetIsFastRoundedCorner(true));
  layer_tree_host_->VerifyAndClearExpectations();

  commit_state = layer_tree_host_->WillCommit(/*completion=*/nullptr,
                                              /*has_updates=*/true);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      top->PushPropertiesTo(top_impl.get(), *commit_state, unsafe_state);
      child->PushPropertiesTo(child_impl.get(), *commit_state, unsafe_state);
      child2->PushPropertiesTo(child2_impl.get(), *commit_state, unsafe_state);
      grand_child->PushPropertiesTo(grand_child_impl.get(), *commit_state,
                                    unsafe_state));
  layer_tree_host_->CommitComplete(commit_state->source_frame_number,
                                   {base::TimeTicks(), base::TimeTicks::Now()});

  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit()).Times(1);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(top->SetHideLayerAndSubtree(true));
  layer_tree_host_->VerifyAndClearExpectations();

  commit_state = layer_tree_host_->WillCommit(/*completion=*/nullptr,
                                              /*has_updates=*/true);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      top->PushPropertiesTo(top_impl.get(), *commit_state, unsafe_state);
      child->PushPropertiesTo(child_impl.get(), *commit_state, unsafe_state);
      child2->PushPropertiesTo(child2_impl.get(), *commit_state, unsafe_state);
      grand_child->PushPropertiesTo(grand_child_impl.get(), *commit_state,
                                    unsafe_state));
  layer_tree_host_->CommitComplete(commit_state->source_frame_number,
                                   {base::TimeTicks(), base::TimeTicks::Now()});

  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit()).Times(1);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(top->SetBlendMode(arbitrary_blend_mode));
  layer_tree_host_->VerifyAndClearExpectations();

  commit_state = layer_tree_host_->WillCommit(/*completion=*/nullptr,
                                              /*has_updates=*/true);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      top->PushPropertiesTo(top_impl.get(), *commit_state, unsafe_state);
      child->PushPropertiesTo(child_impl.get(), *commit_state, unsafe_state);
      child2->PushPropertiesTo(child2_impl.get(), *commit_state, unsafe_state);
      grand_child->PushPropertiesTo(grand_child_impl.get(), *commit_state,
                                    unsafe_state));
  layer_tree_host_->CommitComplete(commit_state->source_frame_number,
                                   {base::TimeTicks(), base::TimeTicks::Now()});

  // Should be a different size than previous call, to ensure it marks tree
  // changed.
  arbitrary_size = gfx::Size(111, 222);
  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit()).Times(2);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(top->SetBounds(arbitrary_size));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(mask_layer1->SetBounds(arbitrary_size));
  layer_tree_host_->VerifyAndClearExpectations();

  commit_state = layer_tree_host_->WillCommit(/*completion=*/nullptr,
                                              /*has_updates=*/true);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      top->PushPropertiesTo(top_impl.get(), *commit_state, unsafe_state);
      child->PushPropertiesTo(child_impl.get(), *commit_state, unsafe_state);
      child2->PushPropertiesTo(child2_impl.get(), *commit_state, unsafe_state);
      grand_child->PushPropertiesTo(grand_child_impl.get(), *commit_state,
                                    unsafe_state));
  layer_tree_host_->CommitComplete(commit_state->source_frame_number,
                                   {base::TimeTicks(), base::TimeTicks::Now()});

  FilterOperations arbitrary_filters;
  arbitrary_filters.Append(FilterOperation::CreateOpacityFilter(0.5f));
  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit()).Times(1);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(top->SetFilters(arbitrary_filters));
  layer_tree_host_->VerifyAndClearExpectations();

  commit_state = layer_tree_host_->WillCommit(/*completion=*/nullptr,
                                              /*has_updates=*/true);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      top->PushPropertiesTo(top_impl.get(), *commit_state, unsafe_state);
      child->PushPropertiesTo(child_impl.get(), *commit_state, unsafe_state);
      child2->PushPropertiesTo(child2_impl.get(), *commit_state, unsafe_state);
      grand_child->PushPropertiesTo(grand_child_impl.get(), *commit_state,
                                    unsafe_state));
  layer_tree_host_->CommitComplete(commit_state->source_frame_number,
                                   {base::TimeTicks(), base::TimeTicks::Now()});

  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit()).Times(2);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(
      top->SetBackdropFilters(arbitrary_filters));

  commit_state = layer_tree_host_->WillCommit(/*completion=*/nullptr,
                                              /*has_updates=*/true);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      top->PushPropertiesTo(top_impl.get(), *commit_state, unsafe_state);
      child->PushPropertiesTo(child_impl.get(), *commit_state, unsafe_state);
      child2->PushPropertiesTo(child2_impl.get(), *commit_state, unsafe_state);
      grand_child->PushPropertiesTo(grand_child_impl.get(), *commit_state,
                                    unsafe_state));
  layer_tree_host_->CommitComplete(commit_state->source_frame_number,
                                   {base::TimeTicks(), base::TimeTicks::Now()});
  layer_tree_host_->VerifyAndClearExpectations();

  gfx::PointF arbitrary_point_f = gfx::PointF(0.125f, 0.25f);
  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit()).Times(1);
  top->SetPosition(arbitrary_point_f);
  TransformNode* node =
      layer_tree_host_->property_trees()->transform_tree_mutable().Node(
          top->transform_tree_index());
  EXPECT_TRUE(node->transform_changed);

  commit_state = layer_tree_host_->WillCommit(/*completion=*/nullptr,
                                              /*has_updates=*/true);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      top->PushPropertiesTo(top_impl.get(), *commit_state, unsafe_state);
      child->PushPropertiesTo(child_impl.get(), *commit_state, unsafe_state);
      child2->PushPropertiesTo(child2_impl.get(), *commit_state, unsafe_state);
      grand_child->PushPropertiesTo(grand_child_impl.get(), *commit_state,
                                    unsafe_state);
      layer_tree_host_->property_trees()->ResetAllChangeTracking());
  layer_tree_host_->CommitComplete(commit_state->source_frame_number,
                                   {base::TimeTicks(), base::TimeTicks::Now()});
  EXPECT_FALSE(node->transform_changed);
  layer_tree_host_->VerifyAndClearExpectations();

  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit()).Times(1);
  child->SetPosition(arbitrary_point_f);
  node = layer_tree_host_->property_trees()->transform_tree_mutable().Node(
      child->transform_tree_index());
  EXPECT_TRUE(node->transform_changed);
  layer_tree_host_->VerifyAndClearExpectations();

  commit_state = layer_tree_host_->WillCommit(/*completion=*/nullptr,
                                              /*has_updates=*/true);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      child->PushPropertiesTo(child_impl.get(), *commit_state, unsafe_state);
      grand_child->PushPropertiesTo(grand_child_impl.get(), *commit_state,
                                    unsafe_state);
      layer_tree_host_->property_trees()->ResetAllChangeTracking());
  layer_tree_host_->CommitComplete(commit_state->source_frame_number,
                                   {base::TimeTicks(), base::TimeTicks::Now()});
  node = layer_tree_host_->property_trees()->transform_tree_mutable().Node(
      child->transform_tree_index());
  EXPECT_FALSE(node->transform_changed);

  gfx::Point3F arbitrary_point_3f = gfx::Point3F(0.125f, 0.25f, 0.f);
  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit()).Times(1);
  top->SetTransformOrigin(arbitrary_point_3f);
  node = layer_tree_host_->property_trees()->transform_tree_mutable().Node(
      top->transform_tree_index());
  EXPECT_TRUE(node->transform_changed);
  layer_tree_host_->VerifyAndClearExpectations();

  commit_state = layer_tree_host_->WillCommit(/*completion=*/nullptr,
                                              /*has_updates=*/true);
  EXECUTE_AND_VERIFY_SUBTREE_CHANGES_RESET(
      top->PushPropertiesTo(top_impl.get(), *commit_state, unsafe_state);
      child->PushPropertiesTo(child_impl.get(), *commit_state, unsafe_state);
      child2->PushPropertiesTo(child2_impl.get(), *commit_state, unsafe_state);
      grand_child->PushPropertiesTo(grand_child_impl.get(), *commit_state,
                                    unsafe_state);
      layer_tree_host_->property_trees()->ResetAllChangeTracking());
  layer_tree_host_->CommitComplete(commit_state->source_frame_number,
                                   {base::TimeTicks(), base::TimeTicks::Now()});

  gfx::Transform arbitrary_transform;
  arbitrary_transform.Scale3d(0.1f, 0.2f, 0.3f);
  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit()).Times(1);
  top->SetTransform(arbitrary_transform);
  node = layer_tree_host_->property_trees()->transform_tree_mutable().Node(
      top->transform_tree_index());
  EXPECT_TRUE(node->transform_changed);
  layer_tree_host_->VerifyAndClearExpectations();
}

TEST_F(LayerTest, AddAndRemoveChild) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();

  // Upon creation, layers should not have children or parent.
  ASSERT_EQ(0U, parent->children().size());
  EXPECT_FALSE(child->parent());

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_host_->SetRootLayer(parent));
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->AddChild(child));

  ASSERT_EQ(1U, parent->children().size());
  EXPECT_EQ(child.get(), parent->children()[0]);
  EXPECT_EQ(parent.get(), child->parent());
  EXPECT_EQ(parent.get(), child->RootLayer());

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(AtLeast(1), child->RemoveFromParent());
}

TEST_F(LayerTest, SetMaskLayer) {
  scoped_refptr<Layer> parent = Layer::Create();
  FakeContentLayerClient client;
  scoped_refptr<PictureLayer> mask = PictureLayer::Create(&client);
  mask->SetPosition(gfx::PointF(88, 99));

  parent->SetMaskLayer(mask);
  ASSERT_EQ(1u, parent->children().size());
  EXPECT_EQ(parent.get(), mask->parent());
  EXPECT_EQ(mask.get(), parent->children()[0]);
  EXPECT_TRUE(parent->mask_layer());

  // Should ignore mask layer's position.
  EXPECT_TRUE(mask->position().IsOrigin());
  mask->SetPosition(gfx::PointF(11, 22));
  EXPECT_TRUE(mask->position().IsOrigin());

  parent->SetMaskLayer(mask);
  ASSERT_EQ(1u, parent->children().size());
  EXPECT_EQ(parent.get(), mask->parent());
  EXPECT_EQ(mask.get(), parent->children()[0]);
  EXPECT_TRUE(parent->mask_layer());

  scoped_refptr<PictureLayer> mask2 = PictureLayer::Create(&client);
  parent->SetMaskLayer(mask2);
  EXPECT_FALSE(mask->parent());
  ASSERT_EQ(1u, parent->children().size());
  EXPECT_EQ(parent.get(), mask2->parent());
  EXPECT_EQ(mask2.get(), parent->children()[0]);
  EXPECT_TRUE(parent->mask_layer());

  parent->SetMaskLayer(nullptr);
  EXPECT_EQ(0u, parent->children().size());
  EXPECT_FALSE(mask2->parent());
  EXPECT_FALSE(parent->mask_layer());
}

TEST_F(LayerTest, RemoveMaskLayerFromParent) {
  scoped_refptr<Layer> parent = Layer::Create();
  FakeContentLayerClient client;
  scoped_refptr<PictureLayer> mask = PictureLayer::Create(&client);

  parent->SetMaskLayer(mask);
  mask->RemoveFromParent();
  EXPECT_EQ(0u, parent->children().size());
  EXPECT_FALSE(mask->parent());
  EXPECT_FALSE(parent->mask_layer());

  scoped_refptr<PictureLayer> mask2 = PictureLayer::Create(&client);
  parent->SetMaskLayer(mask2);
  EXPECT_TRUE(parent->mask_layer());
}

TEST_F(LayerTest, AddChildAfterSetMaskLayer) {
  scoped_refptr<Layer> parent = Layer::Create();
  FakeContentLayerClient client;
  scoped_refptr<PictureLayer> mask = PictureLayer::Create(&client);
  parent->SetMaskLayer(mask);
  EXPECT_TRUE(parent->mask_layer());

  parent->AddChild(Layer::Create());
  EXPECT_EQ(mask.get(), parent->children().back().get());
  EXPECT_TRUE(parent->mask_layer());

  parent->InsertChild(Layer::Create(), parent->children().size());
  EXPECT_EQ(mask.get(), parent->children().back().get());
  EXPECT_TRUE(parent->mask_layer());
}

TEST_F(LayerTest, AddSameChildTwice) {
  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsFullTreeSync())
      .Times(AtLeast(1));

  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();

  layer_tree_host_->SetRootLayer(parent);

  ASSERT_EQ(0u, parent->children().size());

  parent->AddChild(child);
  ASSERT_EQ(1u, parent->children().size());
  EXPECT_EQ(parent.get(), child->parent());

  parent->AddChild(child);
  ASSERT_EQ(1u, parent->children().size());
  EXPECT_EQ(parent.get(), child->parent());
}

TEST_F(LayerTest, ReorderChildren) {
  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsFullTreeSync())
      .Times(AtLeast(1));

  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child1 = Layer::Create();
  scoped_refptr<Layer> child2 = Layer::Create();
  scoped_refptr<Layer> child3 = Layer::Create();

  layer_tree_host_->SetRootLayer(parent);

  parent->AddChild(child1);
  parent->AddChild(child2);
  parent->AddChild(child3);
  EXPECT_EQ(child1, parent->children()[0]);
  EXPECT_EQ(child2, parent->children()[1]);
  EXPECT_EQ(child3, parent->children()[2]);

  // This is normally done by TreeSynchronizer::PushLayerProperties().
  layer_tree_host_->GetPendingCommitState()
      ->layers_that_should_push_properties.clear();

  LayerList new_children_order;
  new_children_order.emplace_back(child3);
  new_children_order.emplace_back(child1);
  new_children_order.emplace_back(child2);
  parent->ReorderChildren(&new_children_order);
  EXPECT_EQ(child3, parent->children()[0]);
  EXPECT_EQ(child1, parent->children()[1]);
  EXPECT_EQ(child2, parent->children()[2]);

  for (const auto& child : parent->children()) {
    EXPECT_FALSE(base::Contains(layer_tree_host_->GetPendingCommitState()
                                    ->layers_that_should_push_properties,
                                child.get()));
    EXPECT_TRUE(child->subtree_property_changed());
  }
}

TEST_F(LayerTest, InsertChild) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child1 = Layer::Create();
  scoped_refptr<Layer> child2 = Layer::Create();
  scoped_refptr<Layer> child3 = Layer::Create();
  scoped_refptr<Layer> child4 = Layer::Create();

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_host_->SetRootLayer(parent));

  ASSERT_EQ(0U, parent->children().size());

  // Case 1: inserting to empty list.
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->InsertChild(child3, 0));
  ASSERT_EQ(1U, parent->children().size());
  EXPECT_EQ(child3, parent->children()[0]);
  EXPECT_EQ(parent.get(), child3->parent());

  // Case 2: inserting to beginning of list
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->InsertChild(child1, 0));
  ASSERT_EQ(2U, parent->children().size());
  EXPECT_EQ(child1, parent->children()[0]);
  EXPECT_EQ(child3, parent->children()[1]);
  EXPECT_EQ(parent.get(), child1->parent());

  // Case 3: inserting to middle of list
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->InsertChild(child2, 1));
  ASSERT_EQ(3U, parent->children().size());
  EXPECT_EQ(child1, parent->children()[0]);
  EXPECT_EQ(child2, parent->children()[1]);
  EXPECT_EQ(child3, parent->children()[2]);
  EXPECT_EQ(parent.get(), child2->parent());

  // Case 4: inserting to end of list
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->InsertChild(child4, 3));

  ASSERT_EQ(4U, parent->children().size());
  EXPECT_EQ(child1, parent->children()[0]);
  EXPECT_EQ(child2, parent->children()[1]);
  EXPECT_EQ(child3, parent->children()[2]);
  EXPECT_EQ(child4, parent->children()[3]);
  EXPECT_EQ(parent.get(), child4->parent());

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_host_->SetRootLayer(nullptr));
}

TEST_F(LayerTest, InsertChildPastEndOfList) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child1 = Layer::Create();
  scoped_refptr<Layer> child2 = Layer::Create();

  ASSERT_EQ(0U, parent->children().size());

  // insert to an out-of-bounds index
  parent->InsertChild(child1, 53);

  ASSERT_EQ(1U, parent->children().size());
  EXPECT_EQ(child1, parent->children()[0]);

  // insert another child to out-of-bounds, when list is not already empty.
  parent->InsertChild(child2, 2459);

  ASSERT_EQ(2U, parent->children().size());
  EXPECT_EQ(child1, parent->children()[0]);
  EXPECT_EQ(child2, parent->children()[1]);
}

TEST_F(LayerTest, InsertSameChildTwice) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child1 = Layer::Create();
  scoped_refptr<Layer> child2 = Layer::Create();

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_host_->SetRootLayer(parent));

  ASSERT_EQ(0U, parent->children().size());

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->InsertChild(child1, 0));
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->InsertChild(child2, 1));

  ASSERT_EQ(2U, parent->children().size());
  EXPECT_EQ(child1, parent->children()[0]);
  EXPECT_EQ(child2, parent->children()[1]);

  // Inserting the same child again should cause the child to be removed and
  // re-inserted at the new location.
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(AtLeast(1), parent->InsertChild(child1, 1));

  // child1 should now be at the end of the list.
  ASSERT_EQ(2U, parent->children().size());
  EXPECT_EQ(child2, parent->children()[0]);
  EXPECT_EQ(child1, parent->children()[1]);

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_host_->SetRootLayer(nullptr));
}

TEST_F(LayerTest, ReplaceChildWithNewChild) {
  CreateSimpleTestTree();
  scoped_refptr<Layer> child4 = Layer::Create();

  EXPECT_FALSE(child4->parent());

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(AtLeast(1),
                                  parent_->ReplaceChild(child2_.get(), child4));
  EXPECT_FALSE(LayerNeedsDisplay(parent_.get()));
  EXPECT_FALSE(LayerNeedsDisplay(child1_.get()));
  EXPECT_FALSE(LayerNeedsDisplay(child2_.get()));
  EXPECT_FALSE(LayerNeedsDisplay(child3_.get()));
  EXPECT_FALSE(LayerNeedsDisplay(child4.get()));

  ASSERT_EQ(static_cast<size_t>(3), parent_->children().size());
  EXPECT_EQ(child1_, parent_->children()[0]);
  EXPECT_EQ(child4, parent_->children()[1]);
  EXPECT_EQ(child3_, parent_->children()[2]);
  EXPECT_EQ(parent_.get(), child4->parent());

  EXPECT_FALSE(child2_->parent());
}

TEST_F(LayerTest, ReplaceChildWithNewChildThatHasOtherParent) {
  CreateSimpleTestTree();

  // create another simple tree with test_layer and child4.
  scoped_refptr<Layer> test_layer = Layer::Create();
  scoped_refptr<Layer> child4 = Layer::Create();
  test_layer->AddChild(child4);
  ASSERT_EQ(1U, test_layer->children().size());
  EXPECT_EQ(child4, test_layer->children()[0]);
  EXPECT_EQ(test_layer.get(), child4->parent());

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(AtLeast(1),
                                  parent_->ReplaceChild(child2_.get(), child4));

  ASSERT_EQ(3U, parent_->children().size());
  EXPECT_EQ(child1_, parent_->children()[0]);
  EXPECT_EQ(child4, parent_->children()[1]);
  EXPECT_EQ(child3_, parent_->children()[2]);
  EXPECT_EQ(parent_.get(), child4->parent());

  // test_layer should no longer have child4,
  // and child2 should no longer have a parent.
  ASSERT_EQ(0U, test_layer->children().size());
  EXPECT_FALSE(child2_->parent());
}

TEST_F(LayerTest, ReplaceChildWithSameChild) {
  CreateSimpleTestTree();

  // SetNeedsFullTreeSync / SetNeedsCommit should not be called because its
  // the same child.
  parent_->ReplaceChild(child2_.get(), child2_);

  VerifyTestTreeInitialState();
}

TEST_F(LayerTest, RemoveAllChildren) {
  CreateSimpleTestTree();

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(AtLeast(3), parent_->RemoveAllChildren());

  ASSERT_EQ(0U, parent_->children().size());
  EXPECT_FALSE(child1_->parent());
  EXPECT_FALSE(child2_->parent());
  EXPECT_FALSE(child3_->parent());
}

TEST_F(LayerTest, HasAncestor) {
  scoped_refptr<Layer> parent = Layer::Create();
  EXPECT_FALSE(parent->HasAncestor(parent.get()));

  scoped_refptr<Layer> child = Layer::Create();
  parent->AddChild(child);

  EXPECT_FALSE(child->HasAncestor(child.get()));
  EXPECT_TRUE(child->HasAncestor(parent.get()));
  EXPECT_FALSE(parent->HasAncestor(child.get()));

  scoped_refptr<Layer> child_child = Layer::Create();
  child->AddChild(child_child);

  EXPECT_FALSE(child_child->HasAncestor(child_child.get()));
  EXPECT_TRUE(child_child->HasAncestor(parent.get()));
  EXPECT_TRUE(child_child->HasAncestor(child.get()));
  EXPECT_FALSE(parent->HasAncestor(child.get()));
  EXPECT_FALSE(parent->HasAncestor(child_child.get()));
}

TEST_F(LayerTest, GetRootLayerAfterTreeManipulations) {
  CreateSimpleTestTree();

  // For this test we don't care about SetNeedsFullTreeSync calls.
  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsFullTreeSync())
      .Times(AnyNumber());

  scoped_refptr<Layer> child4 = Layer::Create();

  EXPECT_EQ(parent_.get(), parent_->RootLayer());
  EXPECT_EQ(parent_.get(), child1_->RootLayer());
  EXPECT_EQ(parent_.get(), child2_->RootLayer());
  EXPECT_EQ(parent_.get(), child3_->RootLayer());
  EXPECT_EQ(child4.get(), child4->RootLayer());
  EXPECT_EQ(parent_.get(), grand_child1_->RootLayer());
  EXPECT_EQ(parent_.get(), grand_child2_->RootLayer());
  EXPECT_EQ(parent_.get(), grand_child3_->RootLayer());

  child1_->RemoveFromParent();

  // |child1| and its children, grand_child1 and grand_child2 are now on a
  // separate subtree.
  EXPECT_EQ(parent_.get(), parent_->RootLayer());
  EXPECT_EQ(child1_.get(), child1_->RootLayer());
  EXPECT_EQ(parent_.get(), child2_->RootLayer());
  EXPECT_EQ(parent_.get(), child3_->RootLayer());
  EXPECT_EQ(child4.get(), child4->RootLayer());
  EXPECT_EQ(child1_.get(), grand_child1_->RootLayer());
  EXPECT_EQ(child1_.get(), grand_child2_->RootLayer());
  EXPECT_EQ(parent_.get(), grand_child3_->RootLayer());

  grand_child3_->AddChild(child4);

  EXPECT_EQ(parent_.get(), parent_->RootLayer());
  EXPECT_EQ(child1_.get(), child1_->RootLayer());
  EXPECT_EQ(parent_.get(), child2_->RootLayer());
  EXPECT_EQ(parent_.get(), child3_->RootLayer());
  EXPECT_EQ(parent_.get(), child4->RootLayer());
  EXPECT_EQ(child1_.get(), grand_child1_->RootLayer());
  EXPECT_EQ(child1_.get(), grand_child2_->RootLayer());
  EXPECT_EQ(parent_.get(), grand_child3_->RootLayer());

  child2_->ReplaceChild(grand_child3_.get(), child1_);

  // |grand_child3| gets orphaned and the child1 subtree gets planted back
  // into the tree under child2.
  EXPECT_EQ(parent_.get(), parent_->RootLayer());
  EXPECT_EQ(parent_.get(), child1_->RootLayer());
  EXPECT_EQ(parent_.get(), child2_->RootLayer());
  EXPECT_EQ(parent_.get(), child3_->RootLayer());
  EXPECT_EQ(grand_child3_.get(), child4->RootLayer());
  EXPECT_EQ(parent_.get(), grand_child1_->RootLayer());
  EXPECT_EQ(parent_.get(), grand_child2_->RootLayer());
  EXPECT_EQ(grand_child3_.get(), grand_child3_->RootLayer());
}

TEST_F(LayerTest, CheckSetNeedsDisplayCausesCorrectBehavior) {
  // The semantics for SetNeedsDisplay which are tested here:
  //   1. sets NeedsDisplay flag appropriately.
  //   2. indirectly calls SetNeedsUpdate, exactly once for each call to
  //      SetNeedsDisplay.

  scoped_refptr<Layer> test_layer = Layer::Create();
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1,
                                  layer_tree_host_->SetRootLayer(test_layer));
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(test_layer->SetIsDrawable(true));

  gfx::Size test_bounds = gfx::Size(501, 508);

  gfx::Rect dirty_rect = gfx::Rect(10, 15, 1, 2);
  gfx::Rect out_of_bounds_dirty_rect = gfx::Rect(400, 405, 500, 502);

  // Before anything, test_layer should not be dirty.
  EXPECT_FALSE(LayerNeedsDisplay(test_layer.get()));

  // This is just initialization, but SetNeedsCommit behavior is verified
  // anyway to avoid warnings.
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(test_layer->SetBounds(test_bounds));
  EXPECT_FALSE(LayerNeedsDisplay(test_layer.get()));

  // The real test begins here.
  SimulateCommitForLayer(test_layer.get());
  EXPECT_FALSE(LayerNeedsDisplay(test_layer.get()));

  // Case 1: Layer should accept dirty rects that go beyond its bounds.
  EXPECT_FALSE(LayerNeedsDisplay(test_layer.get()));
  EXPECT_SET_NEEDS_UPDATE(
      1, test_layer->SetNeedsDisplayRect(out_of_bounds_dirty_rect));
  EXPECT_TRUE(LayerNeedsDisplay(test_layer.get()));
  SimulateCommitForLayer(test_layer.get());

  // Case 2: SetNeedsDisplay() without the dirty rect arg.
  EXPECT_FALSE(LayerNeedsDisplay(test_layer.get()));
  EXPECT_SET_NEEDS_UPDATE(1, test_layer->SetNeedsDisplay());
  EXPECT_TRUE(LayerNeedsDisplay(test_layer.get()));
  SimulateCommitForLayer(test_layer.get());

  // Case 3: SetNeedsDisplay() with an empty rect.
  EXPECT_FALSE(LayerNeedsDisplay(test_layer.get()));
  EXPECT_SET_NEEDS_COMMIT_WAS_NOT_CALLED(
      test_layer->SetNeedsDisplayRect(gfx::Rect()));
  EXPECT_FALSE(LayerNeedsDisplay(test_layer.get()));
  SimulateCommitForLayer(test_layer.get());

  // Case 4: SetNeedsDisplay() with a non-drawable layer
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(test_layer->SetIsDrawable(false));
  SimulateCommitForLayer(test_layer.get());
  EXPECT_FALSE(LayerNeedsDisplay(test_layer.get()));
  EXPECT_SET_NEEDS_UPDATE(0, test_layer->SetNeedsDisplayRect(dirty_rect));
  EXPECT_TRUE(LayerNeedsDisplay(test_layer.get()));
}

TEST_F(LayerTest, CheckPropertyChangeCausesCorrectBehavior) {
  scoped_refptr<Layer> test_layer = Layer::Create();
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1,
                                  layer_tree_host_->SetRootLayer(test_layer));
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(test_layer->SetIsDrawable(true));

  FakeContentLayerClient client;
  scoped_refptr<PictureLayer> mask_layer1 = PictureLayer::Create(&client);

  // sanity check of initial test condition
  EXPECT_FALSE(LayerNeedsDisplay(test_layer.get()));

  // Next, test properties that should call SetNeedsCommit (but not
  // SetNeedsDisplay). All properties need to be set to new values in order for
  // SetNeedsCommit to be called.
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(
      test_layer->SetTransformOrigin(gfx::Point3F(1.23f, 4.56f, 0.f)));
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(
      test_layer->SetBackgroundColor(SkColors::kLtGray));
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(test_layer->SetMasksToBounds(true));
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(
      test_layer->SetClipRect(gfx::Rect(1, 2, 3, 4)));
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(
      test_layer->SetRoundedCorner({1, 2, 3, 4}));
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(test_layer->SetIsFastRoundedCorner(true));
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(test_layer->SetOpacity(0.5f));
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(
      test_layer->SetBlendMode(SkBlendMode::kHue));
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(test_layer->SetContentsOpaque(true));
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(
      test_layer->SetPosition(gfx::PointF(4.f, 9.f)));
  // We can use any layer pointer here since we aren't syncing for real.
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(
      test_layer->SetScrollable(gfx::Size(1, 1)));
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(
      test_layer->SetScrollOffset(gfx::PointF(10, 10)));
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(
      test_layer->SetMainThreadScrollHitTestRegion(
          Region(gfx::Rect(1, 1, 2, 2))));
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(
      test_layer->SetTransform(gfx::Transform::MakeScale(0.0)));
  TouchActionRegion touch_action_region;
  touch_action_region.Union(TouchAction::kNone, gfx::Rect(10, 10));
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(
      test_layer->SetTouchActionRegion(std::move(touch_action_region)));
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(
      test_layer->SetForceRenderSurfaceForTesting(true));
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(test_layer->SetHideLayerAndSubtree(true));
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(test_layer->SetElementId(ElementId(2)));
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(
      test_layer->SetCaptureBounds(viz::RegionCaptureBounds(
          base::flat_map<viz::RegionCaptureCropId, gfx::Rect>{
              {viz::RegionCaptureCropId(123u, 456u),
               gfx::Rect(0, 0, 640, 480)}})));
  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit()).Times(1);
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, test_layer->SetMaskLayer(mask_layer1));
  layer_tree_host_->VerifyAndClearExpectations();
  // The above tests should not have caused a change to the needs_display
  // flag.
  EXPECT_FALSE(LayerNeedsDisplay(test_layer.get()));

  // As layers are removed from the tree, they will cause a tree sync.
  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsFullTreeSync())
      .Times((AnyNumber()));
}

TEST_F(LayerTest, PushPropertiesAccumulatesUpdateRect) {
  scoped_refptr<Layer> test_layer = Layer::Create();
  std::unique_ptr<LayerImpl> impl_layer =
      LayerImpl::Create(host_impl_.active_tree(), 1);

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1,
                                  layer_tree_host_->SetRootLayer(test_layer));

  host_impl_.active_tree()->SetRootLayerForTesting(std::move(impl_layer));
  LayerImpl* impl_layer_ptr = host_impl_.active_tree()->LayerById(1);
  test_layer->SetNeedsDisplayRect(gfx::Rect(5, 5));
  CommitAndPushProperties(test_layer.get(), impl_layer_ptr);
  EXPECT_EQ(gfx::Rect(0, 0, 5, 5), impl_layer_ptr->update_rect());

  // The LayerImpl's update_rect() should be accumulated here, since we did
  // not do anything to clear it.
  test_layer->SetNeedsDisplayRect(gfx::Rect(10, 10, 5, 5));
  CommitAndPushProperties(test_layer.get(), impl_layer_ptr);
  EXPECT_EQ(gfx::Rect(0, 0, 15, 15), impl_layer_ptr->update_rect());

  // If we do clear the LayerImpl side, then the next update_rect() should be
  // fresh without accumulation.
  host_impl_.active_tree()->ResetAllChangeTracking();
  test_layer->SetNeedsDisplayRect(gfx::Rect(10, 10, 5, 5));
  CommitAndPushProperties(test_layer.get(), impl_layer_ptr);
  EXPECT_EQ(gfx::Rect(10, 10, 5, 5), impl_layer_ptr->update_rect());
}

TEST_F(LayerTest, PushPropertiesCausesLayerPropertyChangedForTransform) {
  scoped_refptr<Layer> test_layer = Layer::Create();
  std::unique_ptr<LayerImpl> impl_layer =
      LayerImpl::Create(host_impl_.active_tree(), 1);

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1,
                                  layer_tree_host_->SetRootLayer(test_layer));

  gfx::Transform transform;
  transform.Rotate(45.0);
  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(test_layer->SetTransform(transform));

  EXPECT_FALSE(impl_layer->LayerPropertyChanged());

  CommitAndPushProperties(test_layer.get(), impl_layer.get());

  EXPECT_TRUE(impl_layer->LayerPropertyChanged());
  EXPECT_FALSE(impl_layer->LayerPropertyChangedFromPropertyTrees());
  EXPECT_TRUE(impl_layer->LayerPropertyChangedNotFromPropertyTrees());
}

TEST_F(LayerTest, PushPropertiesCausesLayerPropertyChangedForRoundCorner) {
  scoped_refptr<Layer> test_layer = Layer::Create();
  test_layer->SetMasksToBounds(true);
  std::unique_ptr<LayerImpl> impl_layer =
      LayerImpl::Create(host_impl_.active_tree(), 1);

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1,
                                  layer_tree_host_->SetRootLayer(test_layer));

  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(
      test_layer->SetRoundedCorner({1, 2, 3, 4}));

  EXPECT_FALSE(impl_layer->LayerPropertyChanged());

  CommitAndPushProperties(test_layer.get(), impl_layer.get());

  EXPECT_TRUE(impl_layer->LayerPropertyChanged());
  EXPECT_FALSE(impl_layer->LayerPropertyChangedFromPropertyTrees());
  EXPECT_TRUE(impl_layer->LayerPropertyChangedNotFromPropertyTrees());
}

TEST_F(LayerTest, PushPropertiesCausesLayerPropertyChangedForOpacity) {
  scoped_refptr<Layer> test_layer = Layer::Create();
  std::unique_ptr<LayerImpl> impl_layer =
      LayerImpl::Create(host_impl_.active_tree(), 1);

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1,
                                  layer_tree_host_->SetRootLayer(test_layer));

  EXPECT_SET_NEEDS_COMMIT_WAS_CALLED(test_layer->SetOpacity(0.5f));

  EXPECT_FALSE(impl_layer->LayerPropertyChanged());

  CommitAndPushProperties(test_layer.get(), impl_layer.get());

  EXPECT_TRUE(impl_layer->LayerPropertyChanged());
  EXPECT_FALSE(impl_layer->LayerPropertyChangedFromPropertyTrees());
  EXPECT_TRUE(impl_layer->LayerPropertyChangedNotFromPropertyTrees());
}

TEST_F(LayerTest, MaskHasParent) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  FakeContentLayerClient client;
  scoped_refptr<PictureLayer> mask = PictureLayer::Create(&client);
  scoped_refptr<PictureLayer> mask_replacement = PictureLayer::Create(&client);

  parent->AddChild(child);
  child->SetMaskLayer(mask);

  EXPECT_EQ(parent.get(), child->parent());
  EXPECT_EQ(child.get(), mask->parent());

  child->SetMaskLayer(mask_replacement);
  EXPECT_EQ(nullptr, mask->parent());
  EXPECT_EQ(child.get(), mask_replacement->parent());
}

class LayerTreeHostFactory {
 public:
  std::unique_ptr<LayerTreeHost> Create(MutatorHost* mutator_host) {
    return Create(LayerTreeSettings(), mutator_host);
  }

  std::unique_ptr<LayerTreeHost> Create(LayerTreeSettings settings,
                                        MutatorHost* mutator_host) {
    LayerTreeHost::InitParams params;
    params.client = &client_;
    params.task_graph_runner = &task_graph_runner_;
    params.settings = &settings;
    params.main_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    params.mutator_host = mutator_host;

    return LayerTreeHost::CreateSingleThreaded(&single_thread_client_,
                                               std::move(params));
  }

 private:
  FakeLayerTreeHostClient client_;
  StubLayerTreeHostSingleThreadClient single_thread_client_;
  TestTaskGraphRunner task_graph_runner_;
};

void AssertLayerTreeHostMatchesForSubtree(Layer* layer, LayerTreeHost* host) {
  EXPECT_EQ(host, layer->layer_tree_host());

  for (size_t i = 0; i < layer->children().size(); ++i)
    AssertLayerTreeHostMatchesForSubtree(layer->children()[i].get(), host);
}

class LayerLayerTreeHostTest : public testing::Test {};

TEST_F(LayerLayerTreeHostTest, EnteringTree) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  FakeContentLayerClient client;
  scoped_refptr<PictureLayer> mask = PictureLayer::Create(&client);

  // Set up a detached tree of layers. The host pointer should be nil for
  // these layers.
  parent->AddChild(child);
  child->SetMaskLayer(mask);

  AssertLayerTreeHostMatchesForSubtree(parent.get(), nullptr);

  LayerTreeHostFactory factory;

  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::kMain);
  std::unique_ptr<LayerTreeHost> layer_tree_host =
      factory.Create(animation_host.get());
  // Setting the root layer should set the host pointer for all layers in the
  // tree.
  layer_tree_host->SetRootLayer(parent.get());

  AssertLayerTreeHostMatchesForSubtree(parent.get(), layer_tree_host.get());

  // Clearing the root layer should also clear out the host pointers for all
  // layers in the tree.
  layer_tree_host->SetRootLayer(nullptr);

  AssertLayerTreeHostMatchesForSubtree(parent.get(), nullptr);
}

TEST_F(LayerLayerTreeHostTest, AddingLayerSubtree) {
  scoped_refptr<Layer> parent = Layer::Create();
  LayerTreeHostFactory factory;

  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::kMain);
  std::unique_ptr<LayerTreeHost> layer_tree_host =
      factory.Create(animation_host.get());

  layer_tree_host->SetRootLayer(parent.get());

  EXPECT_EQ(parent->layer_tree_host(), layer_tree_host.get());

  // Adding a subtree to a layer already associated with a host should set the
  // host pointer on all layers in that subtree.
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<Layer> grand_child = Layer::Create();
  child->AddChild(grand_child);

  // Masks should pick up the new host too.
  FakeContentLayerClient client;
  scoped_refptr<PictureLayer> child_mask = PictureLayer::Create(&client);
  child->SetMaskLayer(child_mask);

  parent->AddChild(child);
  AssertLayerTreeHostMatchesForSubtree(parent.get(), layer_tree_host.get());

  layer_tree_host->SetRootLayer(nullptr);
}

TEST_F(LayerLayerTreeHostTest, ChangeHost) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  FakeContentLayerClient client;
  scoped_refptr<PictureLayer> mask = PictureLayer::Create(&client);

  // Same setup as the previous test.
  parent->AddChild(child);
  child->SetMaskLayer(mask);

  LayerTreeHostFactory factory;
  auto animation_host1 = AnimationHost::CreateForTesting(ThreadInstance::kMain);
  std::unique_ptr<LayerTreeHost> first_layer_tree_host =
      factory.Create(animation_host1.get());
  first_layer_tree_host->SetRootLayer(parent.get());

  AssertLayerTreeHostMatchesForSubtree(parent.get(),
                                       first_layer_tree_host.get());

  // Now re-root the tree to a new host (simulating what we do on a context
  // lost event). This should update the host pointers for all layers in the
  // tree.
  auto animation_host2 = AnimationHost::CreateForTesting(ThreadInstance::kMain);
  std::unique_ptr<LayerTreeHost> second_layer_tree_host =
      factory.Create(animation_host2.get());
  second_layer_tree_host->SetRootLayer(parent.get());

  AssertLayerTreeHostMatchesForSubtree(parent.get(),
                                       second_layer_tree_host.get());

  second_layer_tree_host->SetRootLayer(nullptr);
}

TEST_F(LayerLayerTreeHostTest, ChangeHostInSubtree) {
  scoped_refptr<Layer> first_parent = Layer::Create();
  scoped_refptr<Layer> first_child = Layer::Create();
  scoped_refptr<Layer> second_parent = Layer::Create();
  scoped_refptr<Layer> second_child = Layer::Create();
  scoped_refptr<Layer> second_grand_child = Layer::Create();

  // First put all children under the first parent and set the first host.
  first_parent->AddChild(first_child);
  second_child->AddChild(second_grand_child);
  first_parent->AddChild(second_child);

  LayerTreeHostFactory factory;
  auto animation_host1 = AnimationHost::CreateForTesting(ThreadInstance::kMain);
  std::unique_ptr<LayerTreeHost> first_layer_tree_host =
      factory.Create(animation_host1.get());
  first_layer_tree_host->SetRootLayer(first_parent.get());

  AssertLayerTreeHostMatchesForSubtree(first_parent.get(),
                                       first_layer_tree_host.get());

  // Now reparent the subtree starting at second_child to a layer in a
  // different tree.
  auto animation_host2 = AnimationHost::CreateForTesting(ThreadInstance::kMain);
  std::unique_ptr<LayerTreeHost> second_layer_tree_host =
      factory.Create(animation_host2.get());
  second_layer_tree_host->SetRootLayer(second_parent.get());

  second_parent->AddChild(second_child);

  // The moved layer and its children should point to the new host.
  EXPECT_EQ(second_layer_tree_host.get(), second_child->layer_tree_host());
  EXPECT_EQ(second_layer_tree_host.get(),
            second_grand_child->layer_tree_host());

  // Test over, cleanup time.
  first_layer_tree_host->SetRootLayer(nullptr);
  second_layer_tree_host->SetRootLayer(nullptr);
}

TEST_F(LayerLayerTreeHostTest, ReplaceMaskLayer) {
  FakeContentLayerClient client;

  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<PictureLayer> mask = PictureLayer::Create(&client);
  scoped_refptr<Layer> mask_child = Layer::Create();
  scoped_refptr<PictureLayer> mask_replacement = PictureLayer::Create(&client);

  parent->SetMaskLayer(mask);
  mask->AddChild(mask_child);

  LayerTreeHostFactory factory;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::kMain);
  std::unique_ptr<LayerTreeHost> layer_tree_host =
      factory.Create(animation_host.get());
  layer_tree_host->SetRootLayer(parent.get());

  AssertLayerTreeHostMatchesForSubtree(parent.get(), layer_tree_host.get());

  // Replacing the mask should clear out the old mask's subtree's host
  // pointers.
  parent->SetMaskLayer(mask_replacement);
  EXPECT_EQ(nullptr, mask->layer_tree_host());
  EXPECT_EQ(nullptr, mask_child->layer_tree_host());

  // Test over, cleanup time.
  layer_tree_host->SetRootLayer(nullptr);
}

TEST_F(LayerLayerTreeHostTest, DestroyHostWithNonNullRootLayer) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  root->AddChild(child);
  LayerTreeHostFactory factory;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::kMain);
  std::unique_ptr<LayerTreeHost> layer_tree_host =
      factory.Create(animation_host.get());
  layer_tree_host->SetRootLayer(root);
}

TEST_F(LayerTest, SafeOpaqueBackgroundColor) {
  LayerTreeHostFactory factory;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::kMain);
  std::unique_ptr<LayerTreeHost> layer_tree_host =
      factory.Create(animation_host.get());

  scoped_refptr<Layer> layer = Layer::Create();
  layer_tree_host->SetRootLayer(layer);

  for (int contents_opaque = 0; contents_opaque < 2; ++contents_opaque) {
    for (int layer_opaque = 0; layer_opaque < 2; ++layer_opaque) {
      for (int host_opaque = 0; host_opaque < 2; ++host_opaque) {
        layer->SetContentsOpaque(!!contents_opaque);
        layer->SetBackgroundColor(layer_opaque ? SkColors::kRed
                                               : SkColors::kTransparent);
        layer_tree_host->set_background_color(
            host_opaque ? SkColors::kRed : SkColors::kTransparent);

        layer_tree_host->property_trees()->set_needs_rebuild(true);
        layer_tree_host->BuildPropertyTreesForTesting();
        EXPECT_EQ(contents_opaque,
                  layer->SafeOpaqueBackgroundColor().isOpaque())
            << "Flags: " << contents_opaque << ", " << layer_opaque << ", "
            << host_opaque << "\n";
      }
    }
  }
}

class DrawsContentChangeLayer : public Layer {
 public:
  static scoped_refptr<DrawsContentChangeLayer> Create() {
    return base::WrapRefCounted(new DrawsContentChangeLayer());
  }

  void SetLayerTreeHost(LayerTreeHost* host) override {
    Layer::SetLayerTreeHost(host);
    SetFakeDrawsContent(!fake_draws_content_);
  }

  bool HasDrawableContent() const override {
    return fake_draws_content_ && Layer::HasDrawableContent();
  }

  void SetFakeDrawsContent(bool fake_draws_content) {
    fake_draws_content_ = fake_draws_content;
    UpdateDrawsContent();
  }

 private:
  DrawsContentChangeLayer() : fake_draws_content_(false) {}
  ~DrawsContentChangeLayer() override = default;

  bool fake_draws_content_;
};

TEST_F(LayerTest, DrawsContentChangedInSetLayerTreeHost) {
  scoped_refptr<Layer> root_layer = Layer::Create();
  scoped_refptr<DrawsContentChangeLayer> becomes_not_draws_content =
      DrawsContentChangeLayer::Create();
  scoped_refptr<DrawsContentChangeLayer> becomes_draws_content =
      DrawsContentChangeLayer::Create();
  root_layer->SetIsDrawable(true);
  becomes_not_draws_content->SetIsDrawable(true);
  becomes_not_draws_content->SetFakeDrawsContent(true);
  EXPECT_EQ(0, root_layer->NumDescendantsThatDrawContent());
  root_layer->AddChild(becomes_not_draws_content);
  EXPECT_EQ(0, root_layer->NumDescendantsThatDrawContent());

  becomes_draws_content->SetIsDrawable(true);
  root_layer->AddChild(becomes_draws_content);
  EXPECT_EQ(1, root_layer->NumDescendantsThatDrawContent());
}

TEST_F(LayerTest, PushUpdatesShouldHitTest) {
  scoped_refptr<Layer> root_layer = Layer::Create();
  std::unique_ptr<LayerImpl> impl_layer =
      LayerImpl::Create(host_impl_.active_tree(), 1);
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1,
                                  layer_tree_host_->SetRootLayer(root_layer));
  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit()).Times(5);

  // A layer that draws content should be hit testable.
  root_layer->SetIsDrawable(true);
  root_layer->SetHitTestable(true);
  CommitAndPushProperties(root_layer.get(), impl_layer.get());
  EXPECT_TRUE(impl_layer->draws_content());
  EXPECT_TRUE(impl_layer->HitTestable());

  // A layer that does not draw content and does not hit test without drawing
  // content should not be hit testable.
  root_layer->SetIsDrawable(false);
  root_layer->SetHitTestable(false);
  CommitAndPushProperties(root_layer.get(), impl_layer.get());
  EXPECT_FALSE(impl_layer->draws_content());
  EXPECT_FALSE(impl_layer->HitTestable());

  // |SetHitTestableWithoutDrawsContent| should cause a layer to become hit
  // testable even though it does not draw content.
  root_layer->SetHitTestable(true);
  CommitAndPushProperties(root_layer.get(), impl_layer.get());
  EXPECT_FALSE(impl_layer->draws_content());
  EXPECT_TRUE(impl_layer->HitTestable());
}

void ReceiveCopyOutputResult(int* result_count,
                             std::unique_ptr<viz::CopyOutputResult> result) {
  ++(*result_count);
}

void ReceiveCopyOutputResultAtomic(
    std::atomic<int>* result_count,
    std::unique_ptr<viz::CopyOutputResult> result) {
  ++(*result_count);
}

TEST_F(LayerTest, DedupesCopyOutputRequestsBySource) {
  scoped_refptr<Layer> layer = Layer::Create();
  std::atomic<int> result_count{0};

  // Create identical requests without the source being set, and expect the
  // layer does not abort either one.
  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA,
          viz::CopyOutputRequest::ResultDestination::kSystemMemory,
          base::BindOnce(&ReceiveCopyOutputResultAtomic,
                         base::Unretained(&result_count)));
  layer->RequestCopyOfOutput(std::move(request));
  // Because RequestCopyOfOutput could run as a PostTask to return results
  // RunUntilIdle() to ensure that the result is not returned yet.
  CCTestSuite::RunUntilIdle();
  EXPECT_EQ(0, result_count.load());
  request = std::make_unique<viz::CopyOutputRequest>(
      viz::CopyOutputRequest::ResultFormat::RGBA,
      viz::CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(&ReceiveCopyOutputResultAtomic,
                     base::Unretained(&result_count)));
  layer->RequestCopyOfOutput(std::move(request));
  // Because RequestCopyOfOutput could run as a PostTask to return results
  // RunUntilIdle() to ensure that the result is not returned yet.
  CCTestSuite::RunUntilIdle();
  EXPECT_EQ(0, result_count.load());

  // When the layer is destroyed, expect both requests to be aborted.
  layer = nullptr;
  // Wait for any posted tasks to run so the results will be returned.
  CCTestSuite::RunUntilIdle();
  EXPECT_EQ(2, result_count.load());

  layer = Layer::Create();

  // Create identical requests, but this time the source is being set.  Expect
  // the first request using |kArbitrarySourceId1| aborts immediately when
  // the second request using |kArbitrarySourceId1| is made.
  int did_receive_first_result_from_this_source = 0;
  request = std::make_unique<viz::CopyOutputRequest>(
      viz::CopyOutputRequest::ResultFormat::RGBA,
      viz::CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(&ReceiveCopyOutputResult,
                     &did_receive_first_result_from_this_source));
  request->set_source(kArbitrarySourceId1);
  layer->RequestCopyOfOutput(std::move(request));
  // Because RequestCopyOfOutput could run as a PostTask to return results
  // RunUntilIdle() to ensure that the result is not returned yet.
  CCTestSuite::RunUntilIdle();
  EXPECT_EQ(0, did_receive_first_result_from_this_source);
  // Make a request from a different source.
  int did_receive_result_from_different_source = 0;
  request = std::make_unique<viz::CopyOutputRequest>(
      viz::CopyOutputRequest::ResultFormat::RGBA,
      viz::CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(&ReceiveCopyOutputResult,
                     &did_receive_result_from_different_source));
  request->set_source(kArbitrarySourceId2);
  layer->RequestCopyOfOutput(std::move(request));
  // Because RequestCopyOfOutput could run as a PostTask to return results
  // RunUntilIdle() to ensure that the result is not returned yet.
  CCTestSuite::RunUntilIdle();
  EXPECT_EQ(0, did_receive_result_from_different_source);
  // Make a request without specifying the source.
  int did_receive_result_from_anonymous_source = 0;
  request = std::make_unique<viz::CopyOutputRequest>(
      viz::CopyOutputRequest::ResultFormat::RGBA,
      viz::CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(&ReceiveCopyOutputResult,
                     &did_receive_result_from_anonymous_source));
  layer->RequestCopyOfOutput(std::move(request));
  // Because RequestCopyOfOutput could run as a PostTask to return results
  // RunUntilIdle() to ensure that the result is not returned yet.
  CCTestSuite::RunUntilIdle();
  EXPECT_EQ(0, did_receive_result_from_anonymous_source);
  // Make the second request from |kArbitrarySourceId1|.
  int did_receive_second_result_from_this_source = 0;
  request = std::make_unique<viz::CopyOutputRequest>(
      viz::CopyOutputRequest::ResultFormat::RGBA,
      viz::CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(&ReceiveCopyOutputResult,
                     &did_receive_second_result_from_this_source));
  request->set_source(kArbitrarySourceId1);
  layer->RequestCopyOfOutput(
      std::move(request));  // First request to be aborted.
  // Wait for any posted tasks to run so the results will be returned.
  CCTestSuite::RunUntilIdle();
  EXPECT_EQ(1, did_receive_first_result_from_this_source);
  EXPECT_EQ(0, did_receive_result_from_different_source);
  EXPECT_EQ(0, did_receive_result_from_anonymous_source);
  EXPECT_EQ(0, did_receive_second_result_from_this_source);

  // When the layer is destroyed, the other three requests should be aborted.
  layer = nullptr;
  // Wait for any posted tasks to run so the results will be returned.
  CCTestSuite::RunUntilIdle();
  EXPECT_EQ(1, did_receive_first_result_from_this_source);
  EXPECT_EQ(1, did_receive_result_from_different_source);
  EXPECT_EQ(1, did_receive_result_from_anonymous_source);
  EXPECT_EQ(1, did_receive_second_result_from_this_source);
}

TEST_F(LayerTest, AnimationSchedulesLayerUpdate) {
  // TODO(weiliangc): This is really a LayerTreeHost unittest by this point,
  // though currently there is no good place for this unittest to go. Move to
  // LayerTreeHost unittest when there is a good setup.
  scoped_refptr<Layer> layer = Layer::Create();
  layer->SetElementId(ElementId(2));
  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_host_->SetRootLayer(layer));
  auto element_id = layer->element_id();

  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsUpdateLayers());
  layer_tree_host_->SetElementOpacityMutated(element_id,
                                             ElementListType::ACTIVE, 0.5f);
  layer_tree_host_->VerifyAndClearExpectations();

  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsUpdateLayers());
  gfx::Transform transform;
  transform.Rotate(45.0);
  layer_tree_host_->SetElementTransformMutated(
      element_id, ElementListType::ACTIVE, transform);
  layer_tree_host_->VerifyAndClearExpectations();

  // Scroll offset animation should not schedule a layer update since it is
  // handled similarly to normal compositor scroll updates.
  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsUpdateLayers()).Times(0);
  layer_tree_host_->SetElementScrollOffsetMutated(
      element_id, ElementListType::ACTIVE, gfx::PointF(10, 10));
  layer_tree_host_->VerifyAndClearExpectations();
}

TEST_F(LayerTest, ElementIdIsPushed) {
  scoped_refptr<Layer> test_layer = Layer::Create();
  std::unique_ptr<LayerImpl> impl_layer =
      LayerImpl::Create(host_impl_.active_tree(), 1);

  EXPECT_SET_NEEDS_FULL_TREE_SYNC(1,
                                  layer_tree_host_->SetRootLayer(test_layer));

  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit()).Times(1);

  test_layer->SetElementId(ElementId(2));
  EXPECT_FALSE(impl_layer->element_id());

  CommitAndPushProperties(test_layer.get(), impl_layer.get());
  EXPECT_EQ(ElementId(2), impl_layer->element_id());
}

TEST_F(LayerTest, SetLayerTreeHostNotUsingLayerListsManagesElementId) {
  scoped_refptr<Layer> test_layer = Layer::Create();
  ElementId element_id = ElementId(2);
  test_layer->SetElementId(element_id);

  // Expect additional calls due to has-animation check and initialization
  // of keyframes.
  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit()).Times(3);
  scoped_refptr<AnimationTimeline> timeline =
      AnimationTimeline::Create(AnimationIdProvider::NextTimelineId());
  animation_host_->AddAnimationTimeline(timeline);

  AddOpacityTransitionToElementWithAnimation(element_id, timeline, 10.0, 1.f,
                                             0.f, false);
  EXPECT_TRUE(animation_host_->IsElementAnimating(element_id));

  EXPECT_EQ(nullptr, layer_tree_host_->LayerByElementId(element_id));
  test_layer->SetLayerTreeHost(layer_tree_host_.get());
  // Layer should now be registered by element id.
  EXPECT_EQ(test_layer, layer_tree_host_->LayerByElementId(element_id));

  // We're expected to remove the animations before calling
  // SetLayerTreeHost(nullptr).
  animation_host_->RemoveAnimationTimeline(timeline);

  test_layer->SetLayerTreeHost(nullptr);
  // Layer should have been un-registered.
  EXPECT_EQ(nullptr, layer_tree_host_->LayerByElementId(element_id));
}

// Triggering a commit to push animation counts and raf presence to the
// compositor is expensive and updated counts can wait until the next
// commit to be pushed. See https://crbug.com/1083244.
TEST_F(LayerTest, PushAnimationCountsLazily) {
  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit()).Times(0);
  animation_host_->SetAnimationCounts(0);
  animation_host_->SetCurrentFrameHadRaf(true);
  animation_host_->SetNextFrameHasPendingRaf(true);
  animation_host_->SetHasSmilAnimation(true);
  EXPECT_FALSE(host_impl_.animation_host()->CurrentFrameHadRAF());
  EXPECT_FALSE(host_impl_.animation_host()->HasSmilAnimation());
  EXPECT_FALSE(animation_host_->needs_push_properties());
  animation_host_->PushPropertiesTo(host_impl_.animation_host(),
                                    *layer_tree_host_->property_trees());
  EXPECT_TRUE(host_impl_.animation_host()->CurrentFrameHadRAF());
  EXPECT_TRUE(host_impl_.animation_host()->HasSmilAnimation());
}

TEST_F(LayerTest, SetElementIdNotUsingLayerLists) {
  scoped_refptr<Layer> test_layer = Layer::Create();
  test_layer->SetLayerTreeHost(layer_tree_host_.get());

  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit()).Times(2);
  ElementId element_id = ElementId(2);
  EXPECT_EQ(nullptr, layer_tree_host_->LayerByElementId(element_id));

  test_layer->SetElementId(element_id);
  // Layer should now be registered by element id.
  EXPECT_EQ(test_layer, layer_tree_host_->LayerByElementId(element_id));

  ElementId other_element_id = ElementId(3);
  test_layer->SetElementId(other_element_id);
  // The layer should have been unregistered from the original element
  // id and registered with the new one.
  EXPECT_EQ(nullptr, layer_tree_host_->LayerByElementId(element_id));
  EXPECT_EQ(test_layer, layer_tree_host_->LayerByElementId(other_element_id));

  test_layer->SetLayerTreeHost(nullptr);
}

// Verifies that when mirror count of the layer is incremented or decremented,
// SetPropertyTreesNeedRebuild() and SetNeedsPushProperties() are called
// appropriately.
TEST_F(LayerTest, UpdateMirrorCount) {
  scoped_refptr<Layer> test_layer = Layer::Create();
  test_layer->SetLayerTreeHost(layer_tree_host_.get());

  // This is normally done by TreeSynchronizer::PushLayerProperties().
  layer_tree_host_->GetPendingCommitState()
      ->layers_that_should_push_properties.clear();

  layer_tree_host_->property_trees()->set_needs_rebuild(false);
  EXPECT_EQ(0, test_layer->mirror_count());
  EXPECT_FALSE(layer_tree_host_->property_trees()->needs_rebuild());
  EXPECT_EQ(0u, layer_tree_host_->GetPendingCommitState()
                    ->layers_that_should_push_properties.size());

  // Incrementing mirror count from zero should trigger property trees
  // rebuild.
  test_layer->IncrementMirrorCount();
  EXPECT_EQ(1, test_layer->mirror_count());
  EXPECT_TRUE(layer_tree_host_->property_trees()->needs_rebuild());
  EXPECT_TRUE(base::Contains(layer_tree_host_->GetPendingCommitState()
                                 ->layers_that_should_push_properties,
                             test_layer.get()));

  layer_tree_host_->GetPendingCommitState()
      ->layers_that_should_push_properties.clear();
  layer_tree_host_->property_trees()->set_needs_rebuild(false);

  // Incrementing mirror count from non-zero should not trigger property trees
  // rebuild.
  test_layer->IncrementMirrorCount();
  EXPECT_EQ(2, test_layer->mirror_count());
  EXPECT_FALSE(layer_tree_host_->property_trees()->needs_rebuild());
  EXPECT_TRUE(base::Contains(layer_tree_host_->GetPendingCommitState()
                                 ->layers_that_should_push_properties,
                             test_layer.get()));

  layer_tree_host_->GetPendingCommitState()
      ->layers_that_should_push_properties.clear();

  // Decrementing mirror count to non-zero should not trigger property trees
  // rebuild.
  test_layer->DecrementMirrorCount();
  EXPECT_EQ(1, test_layer->mirror_count());
  EXPECT_FALSE(layer_tree_host_->property_trees()->needs_rebuild());
  EXPECT_TRUE(base::Contains(layer_tree_host_->GetPendingCommitState()
                                 ->layers_that_should_push_properties,
                             test_layer.get()));

  // Decrementing mirror count to zero should trigger property trees rebuild.
  test_layer->DecrementMirrorCount();
  EXPECT_EQ(0, test_layer->mirror_count());
  EXPECT_TRUE(layer_tree_host_->property_trees()->needs_rebuild());
  EXPECT_TRUE(base::Contains(layer_tree_host_->GetPendingCommitState()
                                 ->layers_that_should_push_properties,
                             test_layer.get()));

  test_layer->SetLayerTreeHost(nullptr);
}

TEST_F(LayerTest, UpdatingCaptureBounds) {
  static const viz::RegionCaptureBounds kEmptyBounds;
  static const viz::RegionCaptureBounds kPopulatedBounds(
      base::flat_map<viz::RegionCaptureCropId, gfx::Rect>{
          {viz::RegionCaptureCropId(123u, 456u), gfx::Rect(0, 0, 640, 480)}});
  static const viz::RegionCaptureBounds kUpdatedBounds(
      base::flat_map<viz::RegionCaptureCropId, gfx::Rect>{
          {viz::RegionCaptureCropId(123u, 456u), gfx::Rect(0, 0, 1280, 720)}});

  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit()).Times(3);

  // We don't track full tree syncs in this test.
  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsFullTreeSync())
      .Times(AtLeast(1));

  scoped_refptr<Layer> layer = Layer::Create();
  layer_tree_host_->SetRootLayer(layer);

  // Clear the updates caused by setting a new root layer.
  layer->ClearSubtreePropertyChangedForTesting();
  layer_tree_host_->property_trees()->set_needs_rebuild(false);

  // An empty bounds when none is currently set should not cause an update.
  layer->SetCaptureBounds(kEmptyBounds);
  EXPECT_FALSE(layer_tree_host_->property_trees()->needs_rebuild());
  EXPECT_FALSE(layer->subtree_property_changed());

  // Setting to a new bounds should cause an update.
  layer->SetCaptureBounds(kPopulatedBounds);
  EXPECT_TRUE(layer_tree_host_->property_trees()->needs_rebuild());
  EXPECT_TRUE(layer->subtree_property_changed());

  // Reset properties.
  layer->ClearSubtreePropertyChangedForTesting();
  layer_tree_host_->property_trees()->set_needs_rebuild(false);

  // Setting to the same bounds should not, however.
  layer->SetCaptureBounds(kPopulatedBounds);
  EXPECT_FALSE(layer_tree_host_->property_trees()->needs_rebuild());
  EXPECT_FALSE(layer->subtree_property_changed());

  // Switching to a differently valued bounds should cause an update.
  layer->SetCaptureBounds(kUpdatedBounds);
  EXPECT_TRUE(layer_tree_host_->property_trees()->needs_rebuild());
  EXPECT_TRUE(layer->subtree_property_changed());

  // Reset properties.
  layer->ClearSubtreePropertyChangedForTesting();
  layer_tree_host_->property_trees()->set_needs_rebuild(false);

  // Finally, setting to empty should cause an update.
  layer->SetCaptureBounds(kEmptyBounds);
  EXPECT_TRUE(layer_tree_host_->property_trees()->needs_rebuild());
  EXPECT_TRUE(layer->subtree_property_changed());
}

TEST_F(LayerTest, UpdatingClipRect) {
  const gfx::Size kRootSize(200, 200);
  const gfx::Vector2dF kParentOffset(10.f, 20.f);
  const gfx::Size kLayerSize(100, 100);
  const gfx::Rect kClipRect(50, 25, 100, 100);
  const gfx::Rect kUpdatedClipRect_1(10, 20, 150, 200);
  const gfx::Rect kUpdatedClipRect_2(20, 20, 50, 100);
  const gfx::Rect kUpdatedClipRect_3(50, 25, 100, 80);
  const gfx::Rect kUpdatedClipRect_4(10, 10, 200, 200);

  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> clipped_1 = Layer::Create();
  scoped_refptr<Layer> clipped_2 = Layer::Create();
  scoped_refptr<Layer> clipped_3 = Layer::Create();
  scoped_refptr<Layer> clipped_4 = Layer::Create();

  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsFullTreeSync())
      .Times(AtLeast(1));
  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit())
      .Times(AtLeast(1));
  layer_tree_host_->SetRootLayer(root);
  root->AddChild(parent);
  parent->AddChild(clipped_1);
  parent->AddChild(clipped_2);
  parent->AddChild(clipped_3);
  parent->AddChild(clipped_4);

  root->SetBounds(kRootSize);
  parent->SetBounds(kRootSize);
  clipped_1->SetBounds(kLayerSize);
  clipped_2->SetBounds(kLayerSize);
  clipped_3->SetBounds(kLayerSize);
  clipped_4->SetBounds(kLayerSize);

  // This should introduce the |offset_from_transform_parent| component.
  parent->SetPosition(gfx::PointF() + kParentOffset);

  clipped_1->SetClipRect(kClipRect);
  clipped_2->SetClipRect(kClipRect);
  clipped_3->SetClipRect(kClipRect);
  clipped_4->SetClipRect(kClipRect);
  EXPECT_EQ(clipped_1->clip_rect(), kClipRect);
  EXPECT_EQ(clipped_2->clip_rect(), kClipRect);
  EXPECT_EQ(clipped_3->clip_rect(), kClipRect);
  EXPECT_EQ(clipped_4->clip_rect(), kClipRect);

  root->layer_tree_host()->BuildPropertyTreesForTesting();
  const ClipNode* node_1 = layer_tree_host_->property_trees()->clip_tree().Node(
      clipped_1->clip_tree_index());
  const ClipNode* node_2 = layer_tree_host_->property_trees()->clip_tree().Node(
      clipped_2->clip_tree_index());
  const ClipNode* node_3 = layer_tree_host_->property_trees()->clip_tree().Node(
      clipped_3->clip_tree_index());
  const ClipNode* node_4 = layer_tree_host_->property_trees()->clip_tree().Node(
      clipped_4->clip_tree_index());

  EXPECT_EQ(gfx::RectF(kClipRect) + kParentOffset, node_1->clip);
  EXPECT_EQ(gfx::RectF(kClipRect) + kParentOffset, node_2->clip);
  EXPECT_EQ(gfx::RectF(kClipRect) + kParentOffset, node_3->clip);
  EXPECT_EQ(gfx::RectF(kClipRect) + kParentOffset, node_4->clip);

  // The following layer properties should result in the layer being clipped
  // to its bounds along with being clipped by the clip rect. Check if the
  // final rect on the clip node is set correctly.

  // Setting clip to layer bounds.
  clipped_1->SetMasksToBounds(true);

  // Setting a mask.
  FakeContentLayerClient client;
  scoped_refptr<PictureLayer> mask = PictureLayer::Create(&client);
  clipped_2->SetMaskLayer(mask);

  // Setting a filter that moves pixels.
  FilterOperations move_pixel_filters;
  move_pixel_filters.Append(
      FilterOperation::CreateBlurFilter(2, SkTileMode::kClamp));
  ASSERT_TRUE(move_pixel_filters.HasFilterThatMovesPixels());
  clipped_3->SetFilters(move_pixel_filters);

  clipped_1->SetClipRect(kUpdatedClipRect_1);
  clipped_2->SetClipRect(kUpdatedClipRect_2);
  clipped_3->SetClipRect(kUpdatedClipRect_3);
  clipped_4->SetClipRect(kUpdatedClipRect_4);

  node_1 = layer_tree_host_->property_trees()->clip_tree().Node(
      clipped_1->clip_tree_index());
  node_2 = layer_tree_host_->property_trees()->clip_tree().Node(
      clipped_2->clip_tree_index());
  node_3 = layer_tree_host_->property_trees()->clip_tree().Node(
      clipped_3->clip_tree_index());
  node_4 = layer_tree_host_->property_trees()->clip_tree().Node(
      clipped_4->clip_tree_index());

  EXPECT_EQ(node_1->clip,
            gfx::IntersectRects(gfx::RectF(kUpdatedClipRect_1),
                                gfx::RectF(gfx::SizeF(kLayerSize))) +
                kParentOffset);
  EXPECT_EQ(node_2->clip,
            gfx::IntersectRects(gfx::RectF(kUpdatedClipRect_2),
                                gfx::RectF(gfx::SizeF(kLayerSize))) +
                kParentOffset);
  EXPECT_EQ(node_3->clip,
            gfx::IntersectRects(gfx::RectF(kUpdatedClipRect_3),
                                gfx::RectF(gfx::SizeF(kLayerSize))) +
                kParentOffset);
  EXPECT_EQ(node_4->clip, gfx::RectF(kUpdatedClipRect_4) + kParentOffset);
}

TEST_F(LayerTest, UpdatingRoundedCorners) {
  const gfx::Size kRootSize(200, 200);
  const gfx::Size kLayerSize(100, 100);
  const gfx::Rect kClipRect(50, 25, 100, 100);
  const gfx::Rect kUpdatedClipRect(10, 20, 30, 40);
  const gfx::RoundedCornersF kRoundedCorners(5);
  const gfx::RoundedCornersF kUpdatedRoundedCorners(10);

  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> layer_1 = Layer::Create();
  scoped_refptr<Layer> layer_2 = Layer::Create();
  scoped_refptr<Layer> layer_3 = Layer::Create();
  scoped_refptr<Layer> layer_4 = Layer::Create();
  scoped_refptr<Layer> layer_5 = Layer::Create();

  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsFullTreeSync())
      .Times(AtLeast(1));
  EXPECT_CALL_MOCK_DELEGATE(*layer_tree_host_, SetNeedsCommit())
      .Times(AtLeast(1));

  layer_tree_host_->SetRootLayer(root);
  root->AddChild(layer_1);
  root->AddChild(layer_2);
  root->AddChild(layer_3);
  root->AddChild(layer_4);
  root->AddChild(layer_5);

  root->SetBounds(kRootSize);
  layer_1->SetBounds(kLayerSize);
  layer_2->SetBounds(kLayerSize);
  layer_3->SetBounds(kLayerSize);
  layer_4->SetBounds(kLayerSize);
  layer_5->SetBounds(kLayerSize);

  layer_1->SetClipRect(kClipRect);
  layer_2->SetClipRect(kClipRect);
  layer_3->SetClipRect(kClipRect);
  layer_4->SetClipRect(kClipRect);
  layer_1->SetRoundedCorner(kRoundedCorners);
  layer_2->SetRoundedCorner(kRoundedCorners);
  layer_3->SetRoundedCorner(kRoundedCorners);
  layer_4->SetRoundedCorner(kRoundedCorners);
  layer_5->SetRoundedCorner(kRoundedCorners);
  EXPECT_EQ(layer_1->corner_radii(), kRoundedCorners);
  EXPECT_EQ(layer_2->corner_radii(), kRoundedCorners);
  EXPECT_EQ(layer_3->corner_radii(), kRoundedCorners);
  EXPECT_EQ(layer_4->corner_radii(), kRoundedCorners);
  EXPECT_EQ(layer_5->corner_radii(), kRoundedCorners);

  root->layer_tree_host()->BuildPropertyTreesForTesting();
  const EffectNode* node_1 =
      layer_tree_host_->property_trees()->effect_tree().Node(
          layer_1->effect_tree_index());
  const EffectNode* node_2 =
      layer_tree_host_->property_trees()->effect_tree().Node(
          layer_2->effect_tree_index());
  const EffectNode* node_3 =
      layer_tree_host_->property_trees()->effect_tree().Node(
          layer_3->effect_tree_index());
  const EffectNode* node_4 =
      layer_tree_host_->property_trees()->effect_tree().Node(
          layer_4->effect_tree_index());
  const EffectNode* node_5 =
      layer_tree_host_->property_trees()->effect_tree().Node(
          layer_5->effect_tree_index());

  EXPECT_EQ(gfx::RRectF(gfx::RectF(kClipRect), kRoundedCorners),
            node_1->mask_filter_info.rounded_corner_bounds());
  EXPECT_EQ(gfx::RRectF(gfx::RectF(kClipRect), kRoundedCorners),
            node_2->mask_filter_info.rounded_corner_bounds());
  EXPECT_EQ(gfx::RRectF(gfx::RectF(kClipRect), kRoundedCorners),
            node_3->mask_filter_info.rounded_corner_bounds());
  EXPECT_EQ(gfx::RRectF(gfx::RectF(kClipRect), kRoundedCorners),
            node_4->mask_filter_info.rounded_corner_bounds());
  EXPECT_EQ(gfx::RRectF(gfx::RectF(gfx::Rect(kLayerSize)), kRoundedCorners),
            node_5->mask_filter_info.rounded_corner_bounds());

  // Setting clip to layer bounds.
  layer_1->SetMasksToBounds(true);

  // Setting a mask.
  FakeContentLayerClient client;
  scoped_refptr<PictureLayer> mask = PictureLayer::Create(&client);
  layer_2->SetMaskLayer(mask);

  layer_1->SetRoundedCorner(kUpdatedRoundedCorners);
  layer_2->SetRoundedCorner(kUpdatedRoundedCorners);
  layer_3->SetRoundedCorner(kUpdatedRoundedCorners);
  // Updates the clip rect instead of rounded corners.
  layer_4->SetClipRect(kUpdatedClipRect);
  layer_5->SetRoundedCorner(kUpdatedRoundedCorners);

  node_1 = layer_tree_host_->property_trees()->effect_tree().Node(
      layer_1->effect_tree_index());
  node_2 = layer_tree_host_->property_trees()->effect_tree().Node(
      layer_2->effect_tree_index());
  node_3 = layer_tree_host_->property_trees()->effect_tree().Node(
      layer_3->effect_tree_index());
  node_4 = layer_tree_host_->property_trees()->effect_tree().Node(
      layer_4->effect_tree_index());
  node_5 = layer_tree_host_->property_trees()->effect_tree().Node(
      layer_5->effect_tree_index());

  EXPECT_EQ(gfx::RRectF(gfx::RectF(gfx::IntersectRects(gfx::Rect(kLayerSize),
                                                       kClipRect)),
                        kUpdatedRoundedCorners),
            node_1->mask_filter_info.rounded_corner_bounds());
  EXPECT_EQ(gfx::RRectF(gfx::RectF(gfx::IntersectRects(gfx::Rect(kLayerSize),
                                                       kClipRect)),
                        kUpdatedRoundedCorners),
            node_2->mask_filter_info.rounded_corner_bounds());
  EXPECT_EQ(gfx::RRectF(gfx::RectF(kClipRect), kUpdatedRoundedCorners),
            node_3->mask_filter_info.rounded_corner_bounds());
  EXPECT_EQ(gfx::RRectF(gfx::RectF(kUpdatedClipRect), kRoundedCorners),
            node_4->mask_filter_info.rounded_corner_bounds());
  EXPECT_EQ(
      gfx::RRectF(gfx::RectF(gfx::Rect(kLayerSize)), kUpdatedRoundedCorners),
      node_5->mask_filter_info.rounded_corner_bounds());
}

}  // namespace
}  // namespace cc
