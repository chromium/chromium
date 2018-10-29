// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/occlusion_tracker.h"

#include <stddef.h>

#include "cc/animation/animation_host.h"
#include "cc/base/math_util.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/paint/filter_operation.h"
#include "cc/paint/filter_operations.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/test_occlusion_tracker.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/single_thread_proxy.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

class TestContentLayerImpl : public LayerImpl {
 public:
  TestContentLayerImpl(LayerTreeImpl* tree_impl, int id)
      : LayerImpl(tree_impl, id), override_opaque_contents_rect_(false) {
    SetDrawsContent(true);
  }

  SimpleEnclosedRegion VisibleOpaqueRegion() const override {
    if (override_opaque_contents_rect_) {
      return SimpleEnclosedRegion(
          gfx::IntersectRects(opaque_contents_rect_, visible_layer_rect()));
    }
    return LayerImpl::VisibleOpaqueRegion();
  }
  void SetOpaqueContentsRect(const gfx::Rect& opaque_contents_rect) {
    override_opaque_contents_rect_ = true;
    opaque_contents_rect_ = opaque_contents_rect;
  }

 private:
  bool override_opaque_contents_rect_;
  gfx::Rect opaque_contents_rect_;
};

class TestOcclusionTrackerWithClip : public TestOcclusionTracker {
 public:
  explicit TestOcclusionTrackerWithClip(const gfx::Rect& viewport_rect)
      : TestOcclusionTracker(viewport_rect) {}

  bool OccludedLayer(const LayerImpl* layer,
                     const gfx::Rect& content_rect) const {
    DCHECK(layer->visible_layer_rect().Contains(content_rect));
    return this->GetCurrentOcclusionForLayer(layer->DrawTransform())
        .IsOccluded(content_rect);
  }

  // Gives an unoccluded sub-rect of |content_rect| in the content space of the
  // layer. Simple wrapper around GetUnoccludedContentRect.
  gfx::Rect UnoccludedLayerContentRect(const LayerImpl* layer,
                                       const gfx::Rect& content_rect) const {
    DCHECK(layer->visible_layer_rect().Contains(content_rect));
    return this->GetCurrentOcclusionForLayer(layer->DrawTransform())
        .GetUnoccludedContentRect(content_rect);
  }

  gfx::Rect UnoccludedSurfaceContentRect(const LayerImpl* layer,
                                         const gfx::Rect& content_rect) const {
    const RenderSurfaceImpl* surface =
        GetRenderSurface(const_cast<LayerImpl*>(layer));
    return this->GetCurrentOcclusionForContributingSurface(
                     surface->draw_transform())
        .GetUnoccludedContentRect(content_rect);
  }
};

class OcclusionTrackerTest : public testing::Test {
 protected:
  explicit OcclusionTrackerTest(bool opaque_layers)
      : opaque_layers_(opaque_layers),
        animation_host_(AnimationHost::CreateForTesting(ThreadInstance::MAIN)),
        host_(FakeLayerTreeHost::Create(&client_,
                                        &task_graph_runner_,
                                        animation_host_.get())),
        next_layer_impl_id_(1) {}

  virtual void RunMyTest() = 0;

  void TearDown() override { DestroyLayers(); }

  TestContentLayerImpl* CreateRoot(const gfx::Transform& transform,
                                   const gfx::PointF& position,
                                   const gfx::Size& bounds) {
    LayerTreeImpl* tree = host_->host_impl()->active_tree();
    int id = next_layer_impl_id_++;
    std::unique_ptr<TestContentLayerImpl> layer(
        new TestContentLayerImpl(tree, id));
    TestContentLayerImpl* layer_ptr = layer.get();
    SetProperties(layer_ptr, transform, position, bounds);

    host_->host_impl()->active_tree()->SetRootLayerForTesting(std::move(layer));

    layer_ptr->test_properties()->force_render_surface = true;
    SetRootLayerOnMainThread(layer_ptr);

    return layer_ptr;
  }

  LayerImpl* CreateLayer(LayerImpl* parent,
                         const gfx::Transform& transform,
                         const gfx::PointF& position,
                         const gfx::Size& bounds) {
    LayerTreeImpl* tree = host_->host_impl()->active_tree();
    int id = next_layer_impl_id_++;
    std::unique_ptr<LayerImpl> layer = LayerImpl::Create(tree, id);
    LayerImpl* layer_ptr = layer.get();
    SetProperties(layer_ptr, transform, position, bounds);
    parent->test_properties()->AddChild(std::move(layer));
    return layer_ptr;
  }

  LayerImpl* CreateSurface(LayerImpl* parent,
                           const gfx::Transform& transform,
                           const gfx::PointF& position,
                           const gfx::Size& bounds) {
    LayerImpl* layer = CreateLayer(parent, transform, position, bounds);
    layer->test_properties()->force_render_surface = true;
    return layer;
  }

  TestContentLayerImpl* CreateDrawingLayer(LayerImpl* parent,
                                           const gfx::Transform& transform,
                                           const gfx::PointF& position,
                                           const gfx::Size& bounds,
                                           bool opaque) {
    LayerTreeImpl* tree = host_->host_impl()->active_tree();
    int id = next_layer_impl_id_++;
    std::unique_ptr<TestContentLayerImpl> layer(
        new TestContentLayerImpl(tree, id));
    TestContentLayerImpl* layer_ptr = layer.get();
    SetProperties(layer_ptr, transform, position, bounds);

    if (opaque_layers_) {
      layer_ptr->SetContentsOpaque(opaque);
    } else {
      layer_ptr->SetContentsOpaque(false);
      if (opaque)
        layer_ptr->SetOpaqueContentsRect(gfx::Rect(bounds));
      else
        layer_ptr->SetOpaqueContentsRect(gfx::Rect());
    }

    parent->test_properties()->AddChild(std::move(layer));
    return layer_ptr;
  }

  LayerImpl* CreateMaskLayer(LayerImpl* owning_layer, const gfx::Size& bounds) {
    LayerTreeImpl* tree = host_->host_impl()->active_tree();
    int id = next_layer_impl_id_++;
    std::unique_ptr<TestContentLayerImpl> layer(
        new TestContentLayerImpl(tree, id));
    TestContentLayerImpl* layer_ptr = layer.get();
    SetProperties(layer_ptr, identity_matrix, gfx::PointF(), bounds);
    SetMask(owning_layer, std::move(layer));
    return layer_ptr;
  }

  TestContentLayerImpl* CreateDrawingSurface(LayerImpl* parent,
                                             const gfx::Transform& transform,
                                             const gfx::PointF& position,
                                             const gfx::Size& bounds,
                                             bool opaque) {
    TestContentLayerImpl* layer =
        CreateDrawingLayer(parent, transform, position, bounds, opaque);
    layer->test_properties()->force_render_surface = true;
    return layer;
  }

  void DestroyLayers() {
    host_->host_impl()->active_tree()->SetRootLayerForTesting(nullptr);
    render_surface_list_impl_.clear();
    mask_layers_.clear();
    layer_iterator_.reset();
  }

  void AddCopyRequest(Layer* layer) {
    layer->RequestCopyOfOutput(viz::CopyOutputRequest::CreateStubForTesting());
  }

  void AddCopyRequest(LayerImpl* layer) {
    layer->test_properties()->copy_requests.push_back(
        viz::CopyOutputRequest::CreateStubForTesting());
  }

  void CalcDrawEtc(TestContentLayerImpl* root) {
    root->layer_tree_impl()->BuildLayerListForTesting();
    DCHECK(root == root->layer_tree_impl()->root_layer_for_testing());

    // These occlusion tests attach and detach layers in multiple
    // iterations, so rebuild property trees every time.
    root->layer_tree_impl()->property_trees()->needs_rebuild = true;

    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root, root->bounds(), &render_surface_list_impl_);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);

    layer_iterator_ = std::make_unique<EffectTreeLayerListIterator>(
        host_->host_impl()->active_tree());
  }

  void EnterLayer(LayerImpl* layer, OcclusionTracker* occlusion) {
    ASSERT_EQ(layer_iterator_->current_layer(), layer);
    ASSERT_TRUE(layer_iterator_->state() ==
                EffectTreeLayerListIterator::State::LAYER);
    occlusion->EnterLayer(*layer_iterator_);
  }

  void LeaveLayer(LayerImpl* layer, OcclusionTracker* occlusion) {
    ASSERT_EQ(layer_iterator_->current_layer(), layer);
    ASSERT_TRUE(layer_iterator_->state() ==
                EffectTreeLayerListIterator::State::LAYER);
    occlusion->LeaveLayer(*layer_iterator_);
    ++(*layer_iterator_);
  }

  void VisitLayer(LayerImpl* layer, OcclusionTracker* occlusion) {
    EnterLayer(layer, occlusion);
    LeaveLayer(layer, occlusion);
  }

  void EnterContributingSurface(LayerImpl* layer, OcclusionTracker* occlusion) {
    ASSERT_EQ(layer_iterator_->target_render_surface(),
              GetRenderSurface(layer));
    ASSERT_TRUE(layer_iterator_->state() ==
                EffectTreeLayerListIterator::State::TARGET_SURFACE);
    occlusion->EnterLayer(*layer_iterator_);
    occlusion->LeaveLayer(*layer_iterator_);
    ++(*layer_iterator_);
    ASSERT_TRUE(layer_iterator_->state() ==
                EffectTreeLayerListIterator::State::CONTRIBUTING_SURFACE);
    occlusion->EnterLayer(*layer_iterator_);
  }

  void LeaveContributingSurface(LayerImpl* layer, OcclusionTracker* occlusion) {
    ASSERT_EQ(layer_iterator_->current_render_surface(),
              GetRenderSurface(layer));
    ASSERT_TRUE(layer_iterator_->state() ==
                EffectTreeLayerListIterator::State::CONTRIBUTING_SURFACE);
    occlusion->LeaveLayer(*layer_iterator_);
    ++(*layer_iterator_);
  }

  void VisitContributingSurface(LayerImpl* layer, OcclusionTracker* occlusion) {
    EnterContributingSurface(layer, occlusion);
    LeaveContributingSurface(layer, occlusion);
  }

  void ResetLayerIterator() {
    *layer_iterator_ =
        EffectTreeLayerListIterator(host_->host_impl()->active_tree());
  }

  const gfx::Transform identity_matrix;

 private:
  void SetRootLayerOnMainThread(Layer* root) {
    host_->SetRootLayer(scoped_refptr<Layer>(root));
  }

  void SetRootLayerOnMainThread(LayerImpl* root) {}

  void SetProperties(LayerImpl* layer,
                     const gfx::Transform& transform,
                     const gfx::PointF& position,
                     const gfx::Size& bounds) {
    layer->test_properties()->transform = transform;
    layer->SetPosition(position);
    layer->SetBounds(bounds);
  }

  void SetMask(LayerImpl* owning_layer, std::unique_ptr<LayerImpl> layer) {
    owning_layer->test_properties()->SetMaskLayer(std::move(layer));
  }

  bool opaque_layers_;
  FakeLayerTreeHostClient client_;
  TestTaskGraphRunner task_graph_runner_;
  std::unique_ptr<AnimationHost> animation_host_;
  std::unique_ptr<FakeLayerTreeHost> host_;
  // These hold ownership of the layers for the duration of the test.
  RenderSurfaceList render_surface_list_impl_;
  std::unique_ptr<EffectTreeLayerListIterator> layer_iterator_;
  LayerList mask_layers_;
  int next_layer_impl_id_;
};

#define RUN_TEST_IMPL_THREAD_OPAQUE_LAYERS(ClassName)          \
  class ClassName##ImplThreadOpaqueLayers : public ClassName { \
   public: /* NOLINT(whitespace/indent) */                     \
    ClassName##ImplThreadOpaqueLayers() : ClassName(true) {}   \
  };                                                           \
  TEST_F(ClassName##ImplThreadOpaqueLayers, RunTest) { RunMyTest(); }
#define RUN_TEST_IMPL_THREAD_OPAQUE_PAINTS(ClassName)          \
  class ClassName##ImplThreadOpaquePaints : public ClassName { \
   public: /* NOLINT(whitespace/indent) */                     \
    ClassName##ImplThreadOpaquePaints() : ClassName(false) {}  \
  };                                                           \
  TEST_F(ClassName##ImplThreadOpaquePaints, RunTest) { RunMyTest(); }

#define ALL_OCCLUSIONTRACKER_TEST(ClassName)                                   \
      RUN_TEST_IMPL_THREAD_OPAQUE_LAYERS(ClassName)                            \
      RUN_TEST_IMPL_THREAD_OPAQUE_PAINTS(ClassName)

class OcclusionTrackerTestIdentityTransforms : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestIdentityTransforms(bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}

  void RunMyTest() override {
    TestContentLayerImpl* root = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(200, 200));
    TestContentLayerImpl* parent = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::PointF(), gfx::Size(100, 100), true);
    TestContentLayerImpl* layer = this->CreateDrawingLayer(
        parent, this->identity_matrix, gfx::PointF(30.f, 30.f),
        gfx::Size(500, 500), true);
    parent->SetMasksToBounds(true);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(layer, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(30, 30, 70, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestIdentityTransforms);

class OcclusionTrackerTestRotatedChild : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestRotatedChild(bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform layer_transform;
    layer_transform.Translate(250.0, 250.0);
    layer_transform.Rotate(90.0);
    layer_transform.Translate(-250.0, -250.0);

    TestContentLayerImpl* root = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(200, 200));
    TestContentLayerImpl* parent = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::PointF(), gfx::Size(100, 100), true);
    TestContentLayerImpl* layer = this->CreateDrawingLayer(
        parent, layer_transform, gfx::PointF(30.f, 30.f), gfx::Size(500, 500),
        true);
    parent->SetMasksToBounds(true);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(layer, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(30, 30, 70, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestRotatedChild);

class OcclusionTrackerTestTranslatedChild : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestTranslatedChild(bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform layer_transform;
    layer_transform.Translate(20.0, 20.0);

    TestContentLayerImpl* root = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(200, 200));
    TestContentLayerImpl* parent = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::PointF(), gfx::Size(100, 100), true);
    TestContentLayerImpl* layer = this->CreateDrawingLayer(
        parent, layer_transform, gfx::PointF(30.f, 30.f), gfx::Size(500, 500),
        true);
    parent->SetMasksToBounds(true);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(layer, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(50, 50, 50, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestTranslatedChild);

class OcclusionTrackerTestChildInRotatedChild : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestChildInRotatedChild(bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform child_transform;
    child_transform.Translate(250.0, 250.0);
    child_transform.Rotate(90.0);
    child_transform.Translate(-250.0, -250.0);

    TestContentLayerImpl* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 100));
    parent->SetMasksToBounds(true);
    LayerImpl* child = this->CreateSurface(
        parent, child_transform, gfx::PointF(30.f, 30.f), gfx::Size(500, 500));
    child->SetMasksToBounds(true);
    TestContentLayerImpl* layer = this->CreateDrawingLayer(
        child, this->identity_matrix, gfx::PointF(10.f, 10.f),
        gfx::Size(500, 500), true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(layer, &occlusion);
    this->EnterContributingSurface(child, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(10, 430, 60, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveContributingSurface(child, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(30, 40, 70, 60).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    /* Justification for the above occlusion from |layer|:
                  100
         +---------------------+
         |                     |
         |    30               |           rotate(90)
         | 30 + ---------------------------------+
     100 |    |  10            |                 |            ==>
         |    |10+---------------------------------+
         |    |  |             |                 | |
         |    |  |             |                 | |
         |    |  |             |                 | |
         +----|--|-------------+                 | |
              |  |                               | |
              |  |                               | |
              |  |                               | |500
              |  |                               | |
              |  |                               | |
              |  |                               | |
              |  |                               | |
              +--|-------------------------------+ |
                 |                                 |
                 +---------------------------------+
                                500

        +---------------------+
        |                     |30  Visible region of |layer|: /////
        |                     |
        |     +---------------------------------+
     100|     |               |10               |
        |  +---------------------------------+  |
        |  |  |///////////////|     420      |  |
        |  |  |///////////////|60            |  |
        |  |  |///////////////|              |  |
        +--|--|---------------+              |  |
         20|10|     70                       |  |
           |  |                              |  |
           |  |                              |  |
           |  |                              |  |
           |  |                              |  |
           |  |                              |  |
           |  |                              |10|
           |  +------------------------------|--+
           |                 490             |
           +---------------------------------+
                          500

     */
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestChildInRotatedChild);

class OcclusionTrackerTestScaledRenderSurface : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestScaledRenderSurface(bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}

  void RunMyTest() override {
    TestContentLayerImpl* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(200, 200));

    gfx::Transform layer1_matrix;
    layer1_matrix.Scale(2.0, 2.0);
    TestContentLayerImpl* layer1 = this->CreateDrawingLayer(
        parent, layer1_matrix, gfx::PointF(), gfx::Size(100, 100), true);
    layer1->test_properties()->force_render_surface = true;

    gfx::Transform layer2_matrix;
    layer2_matrix.Translate(25.0, 25.0);
    TestContentLayerImpl* layer2 = this->CreateDrawingLayer(
        layer1, layer2_matrix, gfx::PointF(), gfx::Size(50, 50), true);
    TestContentLayerImpl* occluder = this->CreateDrawingLayer(
        parent, this->identity_matrix, gfx::PointF(100.f, 100.f),
        gfx::Size(500, 500), true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(occluder, &occlusion);
    this->EnterLayer(layer2, &occlusion);

    EXPECT_EQ(gfx::Rect(100, 100, 100, 100).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestScaledRenderSurface);

class OcclusionTrackerTestVisitTargetTwoTimes : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestVisitTargetTwoTimes(bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    TestContentLayerImpl* root = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(200, 200));
    LayerImpl* surface = this->CreateSurface(
        root, this->identity_matrix, gfx::PointF(30.f, 30.f), gfx::Size());
    TestContentLayerImpl* surface_child = this->CreateDrawingLayer(
        surface, this->identity_matrix, gfx::PointF(10.f, 10.f),
        gfx::Size(50, 50), true);
    // |top_layer| makes |root|'s surface get considered by OcclusionTracker
    // first, instead of |surface|'s. This exercises different code in
    // LeaveToRenderTarget, as the target surface has already been seen when
    // leaving |surface| later.
    TestContentLayerImpl* top_layer = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::PointF(40.f, 90.f), gfx::Size(50, 20),
        true);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(top_layer, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(40, 90, 50, 20).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->VisitLayer(surface_child, &occlusion);

    EXPECT_EQ(gfx::Rect(10, 60, 50, 20).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(10, 10, 50, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->EnterContributingSurface(surface, &occlusion);

    EXPECT_EQ(gfx::Rect(10, 60, 50, 20).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(10, 10, 50, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // Occlusion from |top_layer| already in the root target should get merged
    // with the occlusion from the |surface| we are leaving now.
    this->LeaveContributingSurface(surface, &occlusion);
    this->EnterLayer(root, &occlusion);

    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_EQ(gfx::Rect(40, 40, 50, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestVisitTargetTwoTimes);

class OcclusionTrackerTestSurfaceRotatedOffAxis : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestSurfaceRotatedOffAxis(bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform child_transform;
    child_transform.Translate(250.0, 250.0);
    child_transform.Rotate(95.0);
    child_transform.Translate(-250.0, -250.0);

    gfx::Transform layer_transform;
    layer_transform.Translate(10.0, 10.0);

    TestContentLayerImpl* root = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(1000, 1000));
    TestContentLayerImpl* parent = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::PointF(), gfx::Size(100, 100), true);
    LayerImpl* child = this->CreateSurface(
        parent, child_transform, gfx::PointF(30.f, 30.f), gfx::Size(500, 500));
    TestContentLayerImpl* layer = this->CreateDrawingLayer(
        child, layer_transform, gfx::PointF(), gfx::Size(500, 500), true);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));

    gfx::Rect clipped_layer_in_child = MathUtil::MapEnclosingClippedRect(
        layer_transform, layer->visible_layer_rect());

    this->VisitLayer(layer, &occlusion);
    this->EnterContributingSurface(child, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(clipped_layer_in_child.ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveContributingSurface(child, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestSurfaceRotatedOffAxis);

class OcclusionTrackerTestSurfaceWithTwoOpaqueChildren
    : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestSurfaceWithTwoOpaqueChildren(bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform child_transform;
    child_transform.Translate(250.0, 250.0);
    child_transform.Rotate(90.0);
    child_transform.Translate(-250.0, -250.0);

    TestContentLayerImpl* root = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(1000, 1000));
    TestContentLayerImpl* parent = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::PointF(), gfx::Size(100, 100), true);
    parent->SetMasksToBounds(true);
    TestContentLayerImpl* child = this->CreateDrawingSurface(
        parent, child_transform, gfx::PointF(30.f, 30.f), gfx::Size(500, 500),
        false);
    child->SetMasksToBounds(true);
    TestContentLayerImpl* layer1 = this->CreateDrawingLayer(
        child, this->identity_matrix, gfx::PointF(10.f, 10.f),
        gfx::Size(500, 500), true);
    TestContentLayerImpl* layer2 = this->CreateDrawingLayer(
        child, this->identity_matrix, gfx::PointF(10.f, 450.f),
        gfx::Size(500, 60), true);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(layer2, &occlusion);
    this->VisitLayer(layer1, &occlusion);
    this->VisitLayer(child, &occlusion);
    this->EnterContributingSurface(child, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(10, 430, 60, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveContributingSurface(child, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(30, 40, 70, 60).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    /* Justification for the above occlusion from |layer1| and |layer2|:

           +---------------------+
           |                     |30  Visible region of |layer1|: /////
           |                     |    Visible region of |layer2|: \\\\\
           |     +---------------------------------+
           |     |               |10               |
           |  +---------------+-----------------+  |
           |  |  |\\\\\\\\\\\\|//|     420      |  |
           |  |  |\\\\\\\\\\\\|//|60            |  |
           |  |  |\\\\\\\\\\\\|//|              |  |
           +--|--|------------|--+              |  |
            20|10|     70     |                 |  |
              |  |            |                 |  |
              |  |            |                 |  |
              |  |            |                 |  |
              |  |            |                 |  |
              |  |            |                 |  |
              |  |            |                 |10|
              |  +------------|-----------------|--+
              |               | 490             |
              +---------------+-----------------+
                     60               440
         */
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestSurfaceWithTwoOpaqueChildren);

class OcclusionTrackerTestOverlappingSurfaceSiblings
    : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestOverlappingSurfaceSiblings(bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    TestContentLayerImpl* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 100));
    parent->SetMasksToBounds(true);
    LayerImpl* child1 = this->CreateSurface(
        parent, this->identity_matrix, gfx::PointF(10.f, 0.f), gfx::Size());
    LayerImpl* child2 = this->CreateSurface(
        parent, this->identity_matrix, gfx::PointF(30.f, 0.f), gfx::Size());
    TestContentLayerImpl* layer1 = this->CreateDrawingLayer(
        child1, this->identity_matrix, gfx::PointF(), gfx::Size(40, 50), true);
    TestContentLayerImpl* layer2 = this->CreateDrawingLayer(
        child2, this->identity_matrix, gfx::PointF(10.f, 0.f),
        gfx::Size(40, 50), true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(layer2, &occlusion);
    this->EnterContributingSurface(child2, &occlusion);

    // layer2's occlusion.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(10, 0, 40, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveContributingSurface(child2, &occlusion);
    this->VisitLayer(layer1, &occlusion);
    this->EnterContributingSurface(child1, &occlusion);

    // layer2's occlusion in the target space of layer1.
    EXPECT_EQ(gfx::Rect(30, 0, 40, 50).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    // layer1's occlusion.
    EXPECT_EQ(gfx::Rect(0, 0, 40, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveContributingSurface(child1, &occlusion);
    this->EnterLayer(parent, &occlusion);

    // The occlusion from from layer1 and layer2 is merged.
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_EQ(gfx::Rect(10, 0, 70, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestOverlappingSurfaceSiblings);

class OcclusionTrackerTestOverlappingSurfaceSiblingsWithTwoTransforms
    : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestOverlappingSurfaceSiblingsWithTwoTransforms(
      bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform child1_transform;
    child1_transform.Translate(250.0, 250.0);
    child1_transform.Rotate(-90.0);
    child1_transform.Translate(-250.0, -250.0);

    gfx::Transform child2_transform;
    child2_transform.Translate(250.0, 250.0);
    child2_transform.Rotate(90.0);
    child2_transform.Translate(-250.0, -250.0);

    TestContentLayerImpl* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 100));
    parent->SetMasksToBounds(true);
    LayerImpl* child1 = this->CreateSurface(
        parent, child1_transform, gfx::PointF(30.f, 20.f), gfx::Size(10, 10));
    LayerImpl* child2 = this->CreateDrawingSurface(parent, child2_transform,
                                                   gfx::PointF(20.f, 40.f),
                                                   gfx::Size(10, 10), false);
    TestContentLayerImpl* layer1 = this->CreateDrawingLayer(
        child1, this->identity_matrix, gfx::PointF(-10.f, -20.f),
        gfx::Size(510, 510), true);
    TestContentLayerImpl* layer2 = this->CreateDrawingLayer(
        child2, this->identity_matrix, gfx::PointF(-10.f, -10.f),
        gfx::Size(510, 510), true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(layer2, &occlusion);
    this->EnterLayer(child2, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(-10, 420, 70, 80).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveLayer(child2, &occlusion);
    this->EnterContributingSurface(child2, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(-10, 420, 70, 80).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveContributingSurface(child2, &occlusion);
    this->VisitLayer(layer1, &occlusion);
    this->EnterContributingSurface(child1, &occlusion);

    EXPECT_EQ(gfx::Rect(420, -10, 70, 80).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(420, -20, 80, 90).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveContributingSurface(child1, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(10, 20, 90, 80).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    /* Justification for the above occlusion:
                  100
        +---------------------+
        |20                   |       layer1
       10+----------------------------------+
    100 || 30                 |     layer2  |
        |20+----------------------------------+
        || |                  |             | |
        || |                  |             | |
        || |                  |             | |
        +|-|------------------+             | |
         | |                                | | 510
         | |                            510 | |
         | |                                | |
         | |                                | |
         | |                                | |
         | |                                | |
         | |                520             | |
         +----------------------------------+ |
           |                                  |
           +----------------------------------+
                           510
     */
  }
};

ALL_OCCLUSIONTRACKER_TEST(
    OcclusionTrackerTestOverlappingSurfaceSiblingsWithTwoTransforms);

class OcclusionTrackerTestFilters : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestFilters(bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform layer_transform;
    layer_transform.Translate(250.0, 250.0);
    layer_transform.Rotate(90.0);
    layer_transform.Translate(-250.0, -250.0);

    TestContentLayerImpl* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 100));
    parent->SetMasksToBounds(true);
    TestContentLayerImpl* blur_layer = this->CreateDrawingLayer(
        parent, layer_transform, gfx::PointF(30.f, 30.f), gfx::Size(500, 500),
        true);
    TestContentLayerImpl* opaque_layer = this->CreateDrawingLayer(
        parent, layer_transform, gfx::PointF(30.f, 30.f), gfx::Size(500, 500),
        true);
    TestContentLayerImpl* opacity_layer = this->CreateDrawingLayer(
        parent, layer_transform, gfx::PointF(30.f, 30.f), gfx::Size(500, 500),
        true);

    blur_layer->test_properties()->force_render_surface = true;
    FilterOperations filters;
    filters.Append(FilterOperation::CreateBlurFilter(10.f));
    blur_layer->test_properties()->filters = filters;

    opaque_layer->test_properties()->force_render_surface = true;
    filters.Clear();
    filters.Append(FilterOperation::CreateGrayscaleFilter(0.5f));
    opaque_layer->test_properties()->filters = filters;

    opacity_layer->test_properties()->force_render_surface = true;
    filters.Clear();
    filters.Append(FilterOperation::CreateOpacityFilter(0.5f));
    opacity_layer->test_properties()->filters = filters;

    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));

    // Opacity layer won't contribute to occlusion.
    this->VisitLayer(opacity_layer, &occlusion);
    this->EnterContributingSurface(opacity_layer, &occlusion);

    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());

    // And has nothing to contribute to its parent surface.
    this->LeaveContributingSurface(opacity_layer, &occlusion);
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());

    // Opaque layer will contribute to occlusion.
    this->VisitLayer(opaque_layer, &occlusion);
    this->EnterContributingSurface(opaque_layer, &occlusion);

    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_EQ(gfx::Rect(0, 430, 70, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // And it gets translated to the parent surface.
    this->LeaveContributingSurface(opaque_layer, &occlusion);
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_EQ(gfx::Rect(30, 30, 70, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // The blur layer needs to throw away any occlusion from outside its
    // subtree.
    this->EnterLayer(blur_layer, &occlusion);
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());

    // And it won't contribute to occlusion.
    this->LeaveLayer(blur_layer, &occlusion);
    this->EnterContributingSurface(blur_layer, &occlusion);
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());

    // But the opaque layer's occlusion is preserved on the parent.
    this->LeaveContributingSurface(blur_layer, &occlusion);
    this->EnterLayer(parent, &occlusion);
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_EQ(gfx::Rect(30, 30, 70, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestFilters);

class OcclusionTrackerTestOpaqueContentsRegionEmpty
    : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestOpaqueContentsRegionEmpty(bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    TestContentLayerImpl* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 300));
    TestContentLayerImpl* layer =
        this->CreateDrawingSurface(parent, this->identity_matrix, gfx::PointF(),
                                   gfx::Size(200, 200), false);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));
    this->EnterLayer(layer, &occlusion);

    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());

    this->LeaveLayer(layer, &occlusion);
    this->VisitContributingSurface(layer, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestOpaqueContentsRegionEmpty);

class OcclusionTrackerTestOpaqueContentsRegionNonEmpty
    : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestOpaqueContentsRegionNonEmpty(bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    TestContentLayerImpl* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 300));
    TestContentLayerImpl* layer = this->CreateDrawingLayer(
        parent, this->identity_matrix, gfx::PointF(100.f, 100.f),
        gfx::Size(200, 200), false);
    this->CalcDrawEtc(parent);
    {
      TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));
      layer->SetOpaqueContentsRect(gfx::Rect(0, 0, 100, 100));

      this->ResetLayerIterator();
      this->VisitLayer(layer, &occlusion);
      this->EnterLayer(parent, &occlusion);

      EXPECT_EQ(gfx::Rect(100, 100, 100, 100).ToString(),
                occlusion.occlusion_from_inside_target().ToString());
    }
    {
      TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));
      layer->SetOpaqueContentsRect(gfx::Rect(20, 20, 180, 180));

      this->ResetLayerIterator();
      this->VisitLayer(layer, &occlusion);
      this->EnterLayer(parent, &occlusion);

      EXPECT_EQ(gfx::Rect(120, 120, 180, 180).ToString(),
                occlusion.occlusion_from_inside_target().ToString());
    }
    {
      TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));
      layer->SetOpaqueContentsRect(gfx::Rect(150, 150, 100, 100));

      this->ResetLayerIterator();
      this->VisitLayer(layer, &occlusion);
      this->EnterLayer(parent, &occlusion);

      EXPECT_EQ(gfx::Rect(250, 250, 50, 50).ToString(),
                occlusion.occlusion_from_inside_target().ToString());
    }
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestOpaqueContentsRegionNonEmpty);

class OcclusionTrackerTestLayerBehindCameraDoesNotOcclude
    : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestLayerBehindCameraDoesNotOcclude(
      bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform transform;
    transform.Translate(50.0, 50.0);
    transform.ApplyPerspectiveDepth(100.0);
    transform.Translate3d(0.0, 0.0, 110.0);
    transform.Translate(-50.0, -50.0);

    TestContentLayerImpl* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 100));
    TestContentLayerImpl* layer = this->CreateDrawingLayer(
        parent, transform, gfx::PointF(), gfx::Size(100, 100), true);
    parent->test_properties()->should_flatten_transform = false;
    parent->test_properties()->sorting_context_id = 1;
    layer->test_properties()->should_flatten_transform = false;
    layer->test_properties()->sorting_context_id = 1;
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));

    // The |layer| is entirely behind the camera and should not occlude.
    this->VisitLayer(layer, &occlusion);
    this->EnterLayer(parent, &occlusion);
    EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
  }
};

class OcclusionTrackerTestSurfaceOcclusionTranslatesToParent
    : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestSurfaceOcclusionTranslatesToParent(
      bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform surface_transform;
    surface_transform.Translate(300.0, 300.0);
    surface_transform.Scale(2.0, 2.0);
    surface_transform.Translate(-150.0, -150.0);

    TestContentLayerImpl* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(500, 500));
    TestContentLayerImpl* surface = this->CreateDrawingSurface(
        parent, surface_transform, gfx::PointF(), gfx::Size(300, 300), false);
    TestContentLayerImpl* surface2 = this->CreateDrawingSurface(
        parent, this->identity_matrix, gfx::PointF(50.f, 50.f),
        gfx::Size(300, 300), false);
    surface->SetOpaqueContentsRect(gfx::Rect(0, 0, 200, 200));
    surface2->SetOpaqueContentsRect(gfx::Rect(0, 0, 200, 200));
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(surface2, &occlusion);
    this->VisitContributingSurface(surface2, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(50, 50, 200, 200).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // Clear any stored occlusion.
    occlusion.set_occlusion_from_outside_target(SimpleEnclosedRegion());
    occlusion.set_occlusion_from_inside_target(SimpleEnclosedRegion());

    this->VisitLayer(surface, &occlusion);
    this->VisitContributingSurface(surface, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 400, 400).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(
    OcclusionTrackerTestSurfaceOcclusionTranslatesToParent);

class OcclusionTrackerTestSurfaceOcclusionTranslatesWithClipping
    : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestSurfaceOcclusionTranslatesWithClipping(
      bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    TestContentLayerImpl* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 300));
    parent->SetMasksToBounds(true);
    TestContentLayerImpl* surface =
        this->CreateDrawingSurface(parent, this->identity_matrix, gfx::PointF(),
                                   gfx::Size(500, 300), false);
    surface->SetOpaqueContentsRect(gfx::Rect(0, 0, 400, 200));
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(surface, &occlusion);
    this->VisitContributingSurface(surface, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 300, 200).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(
    OcclusionTrackerTestSurfaceOcclusionTranslatesWithClipping);

class OcclusionTrackerTestSurfaceChildOfSurface : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestSurfaceChildOfSurface(bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    // This test verifies that the surface cliprect does not end up empty and
    // clip away the entire unoccluded rect.

    TestContentLayerImpl* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 200));
    LayerImpl* surface =
        this->CreateDrawingSurface(parent, this->identity_matrix, gfx::PointF(),
                                   gfx::Size(100, 100), false);
    LayerImpl* surface_child = this->CreateDrawingSurface(
        surface, this->identity_matrix, gfx::PointF(0.f, 10.f),
        gfx::Size(100, 50), true);
    LayerImpl* topmost = this->CreateDrawingLayer(
        parent, this->identity_matrix, gfx::PointF(), gfx::Size(100, 50), true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(-100, -100, 1000, 1000));

    // |topmost| occludes everything partially so we know occlusion is happening
    // at all.
    this->VisitLayer(topmost, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->VisitLayer(surface_child, &occlusion);

    // surface_child increases the occlusion in the screen by a narrow sliver.
    EXPECT_EQ(gfx::Rect(0, -10, 100, 50).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    // In its own surface, surface_child is at 0,0 as is its occlusion.
    EXPECT_EQ(gfx::Rect(0, 0, 100, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // The root layer always has a clip rect. So the parent of |surface| has a
    // clip rect. However, the owning layer for |surface| does not mask to
    // bounds, so it doesn't have a clip rect of its own. Thus the parent of
    // |surface_child| exercises different code paths as its parent does not
    // have a clip rect.

    this->EnterContributingSurface(surface_child, &occlusion);
    // The |surface_child| can't occlude its own surface, but occlusion from
    // |topmost| can.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_on_contributing_surface_from_outside_target()
                  .ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 50).ToString(),
              occlusion.occlusion_on_contributing_surface_from_inside_target()
                  .ToString());
    this->LeaveContributingSurface(surface_child, &occlusion);

    // When the surface_child's occlusion is transformed up to its parent, make
    // sure it is not clipped away inappropriately.
    this->EnterLayer(surface, &occlusion);
    EXPECT_EQ(gfx::Rect(0, 0, 100, 50).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 10, 100, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    this->LeaveLayer(surface, &occlusion);

    this->EnterContributingSurface(surface, &occlusion);
    // The occlusion from inside |surface| can't affect the surface, but
    // |topmost| can.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_on_contributing_surface_from_outside_target()
                  .ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 50).ToString(),
              occlusion.occlusion_on_contributing_surface_from_inside_target()
                  .ToString());

    this->LeaveContributingSurface(surface, &occlusion);
    this->EnterLayer(parent, &occlusion);
    // The occlusion in |surface| and without are merged into the parent.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 60).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestSurfaceChildOfSurface);

class OcclusionTrackerTestDontOccludePixelsNeededForBackdropFilter
    : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestDontOccludePixelsNeededForBackdropFilter(
      bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform scale_by_half;
    scale_by_half.Scale(0.5, 0.5);

    FilterOperations filters;
    filters.Append(FilterOperation::CreateBlurFilter(10.f));

    enum Direction {
      LEFT,
      RIGHT,
      TOP,
      BOTTOM,
      LAST_DIRECTION = BOTTOM,
    };

    for (int i = 0; i <= LAST_DIRECTION; ++i) {
      SCOPED_TRACE(i);

      // Make a 50x50 filtered surface that is adjacent to occluding layers
      // which are above it in the z-order in various configurations. The
      // surface is scaled to test that the pixel moving is done in the target
      // space, where the backdrop filter is applied.
      TestContentLayerImpl* parent = this->CreateRoot(
          this->identity_matrix, gfx::PointF(), gfx::Size(200, 200));
      LayerImpl* filtered_surface = this->CreateDrawingLayer(
          parent, scale_by_half, gfx::PointF(50.f, 50.f), gfx::Size(100, 100),
          false);
      filtered_surface->test_properties()->backdrop_filters = filters;
      gfx::Rect occlusion_rect;
      switch (i) {
        case LEFT:
          occlusion_rect = gfx::Rect(0, 0, 50, 200);
          break;
        case RIGHT:
          // This is the right edge; filtered_surface is scaled by half.
          occlusion_rect = gfx::Rect(100, 0, 50, 200);
          break;
        case TOP:
          occlusion_rect = gfx::Rect(0, 0, 200, 50);
          break;
        case BOTTOM:
          // This is the bottom edge; filtered_surface is scaled by half.
          occlusion_rect = gfx::Rect(0, 100, 200, 50);
          break;
      }

      LayerImpl* occluding_layer = this->CreateDrawingLayer(
          parent, this->identity_matrix, gfx::PointF(occlusion_rect.origin()),
          occlusion_rect.size(), true);
      occluding_layer->test_properties()->force_render_surface = false;
      this->CalcDrawEtc(parent);

      TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 200, 200));

      // This layer occludes pixels directly beside the filtered_surface.
      // Because filtered surface blends pixels in a radius, it will need to see
      // some of the pixels (up to radius far) underneath the occluding layers.
      this->VisitLayer(occluding_layer, &occlusion);

      EXPECT_EQ(occlusion_rect.ToString(),
                occlusion.occlusion_from_inside_target().ToString());
      EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());

      this->VisitLayer(filtered_surface, &occlusion);

      // The occlusion is used fully inside the surface.
      gfx::Rect occlusion_inside_surface =
          occlusion_rect - gfx::Vector2d(50, 50);
      EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());
      EXPECT_EQ(occlusion_inside_surface.ToString(),
                occlusion.occlusion_from_outside_target().ToString());

      // The surface has a backdrop blur, so it needs pixels that are
      // currently considered occluded in order to be drawn. The pixels it
      // needs should be removed from the occluded area, so that they are drawn
      // when we get to the parent.
      this->VisitContributingSurface(filtered_surface, &occlusion);
      this->EnterLayer(parent, &occlusion);

      // The spread due to a 10px blur is 30px.
      gfx::Rect expected_occlusion = occlusion_rect;
      switch (i) {
        case LEFT:
          expected_occlusion.Inset(0, 0, 30, 0);
          break;
        case RIGHT:
          expected_occlusion.Inset(30, 0, 0, 0);
          break;
        case TOP:
          expected_occlusion.Inset(0, 0, 0, 30);
          break;
        case BOTTOM:
          expected_occlusion.Inset(0, 30, 0, 0);
          break;
      }

      EXPECT_EQ(expected_occlusion.ToString(),
                occlusion.occlusion_from_inside_target().ToString());
      EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());

      this->DestroyLayers();
    }
  }
};

ALL_OCCLUSIONTRACKER_TEST(
    OcclusionTrackerTestDontOccludePixelsNeededForBackdropFilter);

class OcclusionTrackerTestPixelsNeededForDropShadowBackdropFilter
    : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestPixelsNeededForDropShadowBackdropFilter(
      bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform scale_by_half;
    scale_by_half.Scale(0.5, 0.5);

    FilterOperations filters;
    filters.Append(FilterOperation::CreateDropShadowFilter(gfx::Point(10, 10),
                                                           5, SK_ColorBLACK));

    enum Direction {
      LEFT,
      RIGHT,
      TOP,
      BOTTOM,
      LAST_DIRECTION = BOTTOM,
    };

    for (int i = 0; i <= LAST_DIRECTION; ++i) {
      SCOPED_TRACE(i);

      // Make a 50x50 filtered surface that is adjacent to occluding layers
      // which are above it in the z-order in various configurations. The
      // surface is scaled to test that the pixel moving is done in the target
      // space, where the backdrop filter is applied.
      TestContentLayerImpl* parent = this->CreateRoot(
          this->identity_matrix, gfx::PointF(), gfx::Size(200, 200));
      LayerImpl* filtered_surface = this->CreateDrawingLayer(
          parent, scale_by_half, gfx::PointF(50.f, 50.f), gfx::Size(100, 100),
          false);
      filtered_surface->test_properties()->backdrop_filters = filters;
      gfx::Rect occlusion_rect;
      switch (i) {
        case LEFT:
          occlusion_rect = gfx::Rect(0, 0, 50, 200);
          break;
        case RIGHT:
          // This is the right edge; filtered_surface is scaled by half.
          occlusion_rect = gfx::Rect(100, 0, 50, 200);
          break;
        case TOP:
          occlusion_rect = gfx::Rect(0, 0, 200, 50);
          break;
        case BOTTOM:
          // This is the bottom edge; filtered_surface is scaled by half.
          occlusion_rect = gfx::Rect(0, 100, 200, 50);
          break;
      }

      LayerImpl* occluding_layer = this->CreateDrawingLayer(
          parent, this->identity_matrix, gfx::PointF(occlusion_rect.origin()),
          occlusion_rect.size(), true);
      occluding_layer->test_properties()->force_render_surface = false;
      this->CalcDrawEtc(parent);

      TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 200, 200));

      // This layer occludes pixels directly beside the filtered_surface.
      // Because filtered surface blends pixels in a radius, it will need to see
      // some of the pixels (up to radius far) underneath the occluding layers.
      this->VisitLayer(occluding_layer, &occlusion);

      EXPECT_EQ(occlusion_rect.ToString(),
                occlusion.occlusion_from_inside_target().ToString());
      EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());

      this->VisitLayer(filtered_surface, &occlusion);

      // The occlusion is used fully inside the surface.
      gfx::Rect occlusion_inside_surface =
          occlusion_rect - gfx::Vector2d(50, 50);
      EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());
      EXPECT_EQ(occlusion_inside_surface.ToString(),
                occlusion.occlusion_from_outside_target().ToString());

      // The surface has a backdrop filter, so it needs pixels that are
      // currently considered occluded in order to be drawn. The pixels it
      // needs should be removed from the occluded area, so that they are drawn
      // when we get to the parent.
      this->VisitContributingSurface(filtered_surface, &occlusion);
      this->EnterLayer(parent, &occlusion);

      gfx::Rect expected_occlusion;
      switch (i) {
        case LEFT:
          // The right half of the occlusion is close enough to cast a shadow
          // that would be visible in the backdrop filter. The shadow reaches
          // 3*5 + 10 = 25 pixels to the right.
          expected_occlusion = gfx::Rect(0, 0, 25, 200);
          break;
        case RIGHT:
          // The shadow spreads 3*5 - 10 = 5 pixels to the left, so the
          // occlusion must recede by 5 to account for that.
          expected_occlusion = gfx::Rect(105, 0, 45, 200);
          break;
        case TOP:
          // Similar to LEFT.
          expected_occlusion = gfx::Rect(0, 0, 200, 25);
          break;
        case BOTTOM:
          // Similar to RIGHT.
          expected_occlusion = gfx::Rect(0, 105, 200, 45);
          break;
      }

      EXPECT_EQ(expected_occlusion.ToString(),
                occlusion.occlusion_from_inside_target().ToString());
      EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());

      this->DestroyLayers();
    }
  }
};

ALL_OCCLUSIONTRACKER_TEST(
    OcclusionTrackerTestPixelsNeededForDropShadowBackdropFilter);

class OcclusionTrackerTestTwoBackdropFiltersReduceOcclusionTwice
    : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestTwoBackdropFiltersReduceOcclusionTwice(
      bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform scale_by_half;
    scale_by_half.Scale(0.5, 0.5);

    // Makes two surfaces that completely cover |parent|. The occlusion both
    // above and below the filters will be reduced by each of them.
    TestContentLayerImpl* root = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(75, 75));
    LayerImpl* parent = this->CreateSurface(root, scale_by_half, gfx::PointF(),
                                            gfx::Size(150, 150));
    parent->SetMasksToBounds(true);
    LayerImpl* filtered_surface1 = this->CreateDrawingLayer(
        parent, scale_by_half, gfx::PointF(), gfx::Size(300, 300), false);
    LayerImpl* filtered_surface2 = this->CreateDrawingLayer(
        parent, scale_by_half, gfx::PointF(), gfx::Size(300, 300), false);
    LayerImpl* occluding_layer_above = this->CreateDrawingLayer(
        parent, this->identity_matrix, gfx::PointF(100.f, 100.f),
        gfx::Size(50, 50), true);

    // Filters make the layers own surfaces.
    filtered_surface1->test_properties()->force_render_surface = true;
    filtered_surface2->test_properties()->force_render_surface = true;
    FilterOperations filters;
    filters.Append(FilterOperation::CreateBlurFilter(1.f));
    filtered_surface1->test_properties()->backdrop_filters = filters;
    filtered_surface2->test_properties()->backdrop_filters = filters;

    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(occluding_layer_above, &occlusion);
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(100 / 2, 100 / 2, 50 / 2, 50 / 2).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->VisitLayer(filtered_surface2, &occlusion);
    this->VisitContributingSurface(filtered_surface2, &occlusion);
    this->VisitLayer(filtered_surface1, &occlusion);
    this->VisitContributingSurface(filtered_surface1, &occlusion);

    // Test expectations in the target.
    int blur_outset = 3;
    gfx::Rect expected_occlusion =
        gfx::Rect(100 / 2 + blur_outset * 2, 100 / 2 + blur_outset * 2,
                  50 / 2 - blur_outset * 4, 50 / 2 - blur_outset * 4);
    EXPECT_EQ(expected_occlusion.ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // Test expectations in the screen are the same as in the target, as the
    // render surface is 1:1 with the screen.
    EXPECT_EQ(expected_occlusion.ToString(),
              occlusion.occlusion_from_outside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(
    OcclusionTrackerTestTwoBackdropFiltersReduceOcclusionTwice);

class OcclusionTrackerTestDontReduceOcclusionBelowBackdropFilter
    : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestDontReduceOcclusionBelowBackdropFilter(
      bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform scale_by_half;
    scale_by_half.Scale(0.5, 0.5);

    // Make a 50x50 surface, with a smaller 30x30 layer centered below it.
    // The surface is scaled to test that the pixel moving is done in the target
    // space, where the backdrop filter is applied, and the surface appears at
    // 50, 50.
    TestContentLayerImpl* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 150));
    LayerImpl* behind_surface_layer = this->CreateDrawingLayer(
        parent, this->identity_matrix, gfx::PointF(60.f, 60.f),
        gfx::Size(30, 30), true);
    LayerImpl* filtered_surface =
        this->CreateDrawingLayer(parent, scale_by_half, gfx::PointF(50.f, 50.f),
                                 gfx::Size(100, 100), false);

    // Filters make the layer own a surface.
    filtered_surface->test_properties()->force_render_surface = true;
    FilterOperations filters;
    filters.Append(FilterOperation::CreateBlurFilter(3.f));
    filtered_surface->test_properties()->backdrop_filters = filters;

    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));

    // The surface has a backdrop blur, so it blurs non-opaque pixels below
    // it.
    this->VisitLayer(filtered_surface, &occlusion);
    this->VisitContributingSurface(filtered_surface, &occlusion);

    // The layers behind the surface are not blurred, and their occlusion does
    // not change, until we leave the surface.  So it should not be modified by
    // the filter here.
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());

    // Clear the occlusion so the |behind_surface_layer| can add its occlusion
    // without existing occlusion interfering.
    occlusion.set_occlusion_from_inside_target(SimpleEnclosedRegion());

    this->VisitLayer(behind_surface_layer, &occlusion);

    // The layers behind the surface are not blurred, and their occlusion does
    // not change, until we leave the surface.  So it should not be modified by
    // the filter here.
    gfx::Rect occlusion_behind_surface = gfx::Rect(60, 60, 30, 30);
    EXPECT_EQ(occlusion_behind_surface.ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
  }
};

ALL_OCCLUSIONTRACKER_TEST(
    OcclusionTrackerTestDontReduceOcclusionBelowBackdropFilter);

class OcclusionTrackerTestDontReduceOcclusionIfBackdropFilterIsOccluded
    : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestDontReduceOcclusionIfBackdropFilterIsOccluded(
      bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform scale_by_half;
    scale_by_half.Scale(0.5, 0.5);

    // Make a 50x50 filtered surface that is completely occluded by an opaque
    // layer which is above it in the z-order.  The surface is
    // scaled to test that the pixel moving is done in the target space, where
    // the backdrop filter is applied, and the surface appears at 50, 50.
    TestContentLayerImpl* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(200, 150));
    LayerImpl* filtered_surface =
        this->CreateDrawingLayer(parent, scale_by_half, gfx::PointF(50.f, 50.f),
                                 gfx::Size(100, 100), false);
    LayerImpl* occluding_layer = this->CreateDrawingLayer(
        parent, this->identity_matrix, gfx::PointF(50.f, 50.f),
        gfx::Size(50, 50), true);

    // Filters make the layer own a surface.
    filtered_surface->test_properties()->force_render_surface = true;
    FilterOperations filters;
    filters.Append(FilterOperation::CreateBlurFilter(3.f));
    filtered_surface->test_properties()->backdrop_filters = filters;

    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(occluding_layer, &occlusion);

    this->VisitLayer(filtered_surface, &occlusion);
    {
      // The layers above the filtered surface occlude from outside.
      gfx::Rect occlusion_above_surface = gfx::Rect(0, 0, 50, 50);

      EXPECT_EQ(gfx::Rect().ToString(),
                occlusion.occlusion_from_inside_target().ToString());
      EXPECT_EQ(occlusion_above_surface.ToString(),
                occlusion.occlusion_from_outside_target().ToString());
    }

    // The surface has a backdrop blur, so it blurs non-opaque pixels below
    // it.
    this->VisitContributingSurface(filtered_surface, &occlusion);
    {
      // The filter is completely occluded, so it should not blur anything and
      // reduce any occlusion.
      gfx::Rect occlusion_above_surface = gfx::Rect(50, 50, 50, 50);

      EXPECT_EQ(occlusion_above_surface.ToString(),
                occlusion.occlusion_from_inside_target().ToString());
      EXPECT_EQ(gfx::Rect().ToString(),
                occlusion.occlusion_from_outside_target().ToString());
    }
  }
};

ALL_OCCLUSIONTRACKER_TEST(
    OcclusionTrackerTestDontReduceOcclusionIfBackdropFilterIsOccluded);

class OcclusionTrackerTestReduceOcclusionWhenBkgdFilterIsPartiallyOccluded
    : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestReduceOcclusionWhenBkgdFilterIsPartiallyOccluded(
      bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform scale_by_half;
    scale_by_half.Scale(0.5, 0.5);

    // Make a 50x50 surface which is partially occluded by opaque layers which
    // are above it in the z-order.  The surface is scaled to test that the
    // pixel moving is done in the target space, where the backdrop filter is
    // applied, but the surface appears at 50, 50.
    TestContentLayerImpl* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 150));
    LayerImpl* filtered_surface =
        this->CreateDrawingLayer(parent, scale_by_half, gfx::PointF(50.f, 50.f),
                                 gfx::Size(100, 100), false);
    LayerImpl* above_surface_layer = this->CreateDrawingLayer(
        parent, this->identity_matrix, gfx::PointF(70.f, 50.f),
        gfx::Size(30, 50), true);
    LayerImpl* beside_surface_layer = this->CreateDrawingLayer(
        parent, this->identity_matrix, gfx::PointF(90.f, 40.f),
        gfx::Size(10, 10), true);

    // Filters make the layer own a surface.
    filtered_surface->test_properties()->force_render_surface = true;
    FilterOperations filters;
    filters.Append(FilterOperation::CreateBlurFilter(3.f));
    filtered_surface->test_properties()->backdrop_filters = filters;

    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(beside_surface_layer, &occlusion);
    this->VisitLayer(above_surface_layer, &occlusion);

    // The surface has a backdrop blur, so it blurs non-opaque pixels below
    // it.
    this->VisitLayer(filtered_surface, &occlusion);
    this->VisitContributingSurface(filtered_surface, &occlusion);

    // The filter in the surface is partially unoccluded. Only the unoccluded
    // parts should reduce occlusion.  This means it will push back the
    // occlusion that touches the unoccluded part (occlusion_above___), but
    // it will not touch occlusion_beside____ since that is not beside the
    // unoccluded part of the surface, even though it is beside the occluded
    // part of the surface.
    int blur_outset = 9;
    gfx::Rect occlusion_above_surface =
        gfx::Rect(70 + blur_outset, 50, 30 - blur_outset, 50);
    gfx::Rect occlusion_beside_surface = gfx::Rect(90, 40, 10, 10);

    SimpleEnclosedRegion expected_occlusion;
    expected_occlusion.Union(occlusion_beside_surface);
    expected_occlusion.Union(occlusion_above_surface);

    EXPECT_EQ(expected_occlusion.ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());

    const SimpleEnclosedRegion& actual_occlusion =
        occlusion.occlusion_from_inside_target();
    for (size_t i = 0; i < expected_occlusion.GetRegionComplexity(); ++i) {
      ASSERT_LT(i, actual_occlusion.GetRegionComplexity());
      EXPECT_EQ(expected_occlusion.GetRect(i), actual_occlusion.GetRect(i));
    }
  }
};

ALL_OCCLUSIONTRACKER_TEST(
    OcclusionTrackerTestReduceOcclusionWhenBkgdFilterIsPartiallyOccluded);

class OcclusionTrackerTestBlendModeDoesNotOcclude
    : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestBlendModeDoesNotOcclude(bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    TestContentLayerImpl* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 100));
    LayerImpl* blend_mode_layer = this->CreateDrawingLayer(
        parent, this->identity_matrix, gfx::PointF(0.f, 0.f),
        gfx::Size(100, 100), true);
    LayerImpl* top_layer = this->CreateDrawingLayer(
        parent, this->identity_matrix, gfx::PointF(10.f, 12.f),
        gfx::Size(20, 22), true);

    // Blend mode makes the layer own a surface.
    blend_mode_layer->test_properties()->force_render_surface = true;
    blend_mode_layer->test_properties()->blend_mode = SkBlendMode::kMultiply;

    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(top_layer, &occlusion);
    // |top_layer| occludes.
    EXPECT_EQ(gfx::Rect(10, 12, 20, 22).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());

    this->VisitLayer(blend_mode_layer, &occlusion);
    // |top_layer| and |blend_mode_layer| both occlude, since the blend mode
    // gets applied by blend_mode_layer's render surface, not when drawing the
    // layer itself.
    EXPECT_EQ(gfx::Rect(100, 100).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_EQ(gfx::Rect(10, 12, 20, 22).ToString(),
              occlusion.occlusion_from_outside_target().ToString());

    this->VisitContributingSurface(blend_mode_layer, &occlusion);
    // |top_layer| occludes but not |blend_mode_layer|.
    EXPECT_EQ(gfx::Rect(10, 12, 20, 22).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestBlendModeDoesNotOcclude);

class OcclusionTrackerTestMinimumTrackingSize : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestMinimumTrackingSize(bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    gfx::Size tracking_size(100, 100);
    gfx::Size below_tracking_size(99, 99);

    TestContentLayerImpl* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(400, 400));
    LayerImpl* large = this->CreateDrawingLayer(
        parent, this->identity_matrix, gfx::PointF(), tracking_size, true);
    LayerImpl* small =
        this->CreateDrawingLayer(parent, this->identity_matrix, gfx::PointF(),
                                 below_tracking_size, true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));
    occlusion.set_minimum_tracking_size(tracking_size);

    // The small layer is not tracked because it is too small.
    this->VisitLayer(small, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // The large layer is tracked as it is large enough.
    this->VisitLayer(large, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(tracking_size).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestMinimumTrackingSize);

class OcclusionTrackerTestScaledLayerIsClipped : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestScaledLayerIsClipped(bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform scale_transform;
    scale_transform.Scale(512.0, 512.0);

    TestContentLayerImpl* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(400, 400));
    LayerImpl* clip =
        this->CreateLayer(parent, this->identity_matrix,
                          gfx::PointF(10.f, 10.f), gfx::Size(50, 50));
    clip->SetMasksToBounds(true);
    LayerImpl* scale = this->CreateLayer(clip, scale_transform, gfx::PointF(),
                                         gfx::Size(1, 1));
    LayerImpl* scaled = this->CreateDrawingLayer(
        scale, this->identity_matrix, gfx::PointF(), gfx::Size(500, 500), true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(scaled, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(10, 10, 50, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestScaledLayerIsClipped)

class OcclusionTrackerTestScaledLayerInSurfaceIsClipped
    : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestScaledLayerInSurfaceIsClipped(bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform scale_transform;
    scale_transform.Scale(512.0, 512.0);

    TestContentLayerImpl* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(400, 400));
    LayerImpl* clip =
        this->CreateLayer(parent, this->identity_matrix,
                          gfx::PointF(10.f, 10.f), gfx::Size(50, 50));
    clip->SetMasksToBounds(true);
    LayerImpl* surface = this->CreateDrawingSurface(
        clip, this->identity_matrix, gfx::PointF(), gfx::Size(400, 30), false);
    LayerImpl* scale = this->CreateLayer(surface, scale_transform,
                                         gfx::PointF(), gfx::Size(1, 1));
    LayerImpl* scaled = this->CreateDrawingLayer(
        scale, this->identity_matrix, gfx::PointF(), gfx::Size(500, 500), true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(scaled, &occlusion);
    this->VisitLayer(surface, &occlusion);
    this->VisitContributingSurface(surface, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(10, 10, 50, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestScaledLayerInSurfaceIsClipped)

class OcclusionTrackerTestCopyRequestDoesOcclude : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestCopyRequestDoesOcclude(bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    TestContentLayerImpl* root = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(400, 400));
    TestContentLayerImpl* parent = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::PointF(), gfx::Size(400, 400), true);
    LayerImpl* copy =
        this->CreateLayer(parent, this->identity_matrix, gfx::PointF(100, 0),
                          gfx::Size(200, 400));
    this->AddCopyRequest(copy);
    LayerImpl* copy_child = this->CreateDrawingLayer(
        copy, this->identity_matrix, gfx::PointF(), gfx::Size(200, 400), true);
    LayerImpl* top_layer =
        this->CreateDrawingLayer(root, this->identity_matrix,
                                 gfx::PointF(50, 0), gfx::Size(50, 400), true);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(top_layer, &occlusion);
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(50, 0, 50, 400).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->VisitLayer(copy_child, &occlusion);
    // Layers outside the copy request do not occlude.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(200, 400).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // CopyRequests cause the layer to own a surface.
    this->VisitContributingSurface(copy, &occlusion);

    // The occlusion from the copy should be kept.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(50, 0, 250, 400).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestCopyRequestDoesOcclude)

class OcclusionTrackerTestHiddenCopyRequestDoesNotOcclude
    : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestHiddenCopyRequestDoesNotOcclude(
      bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    TestContentLayerImpl* root = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(400, 400));
    TestContentLayerImpl* parent = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::PointF(), gfx::Size(400, 400), true);
    LayerImpl* hide = this->CreateLayer(parent, this->identity_matrix,
                                        gfx::PointF(), gfx::Size());
    LayerImpl* copy =
        this->CreateLayer(hide, this->identity_matrix, gfx::PointF(100.f, 0.f),
                          gfx::Size(200, 400));
    this->AddCopyRequest(copy);
    LayerImpl* copy_child = this->CreateDrawingLayer(
        copy, this->identity_matrix, gfx::PointF(), gfx::Size(200, 400), true);

    // The |copy| layer is hidden but since it is being copied, it will be
    // drawn.
    hide->test_properties()->hide_layer_and_subtree = true;

    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(copy_child, &occlusion);
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(200, 400).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // CopyRequests cause the layer to own a surface.
    this->VisitContributingSurface(copy, &occlusion);

    // The occlusion from the copy should be dropped since it is hidden.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestHiddenCopyRequestDoesNotOcclude)

class OcclusionTrackerTestOccludedLayer : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestOccludedLayer(bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform translate;
    translate.Translate(10.0, 20.0);
    TestContentLayerImpl* root = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(200, 200));
    LayerImpl* surface = this->CreateSurface(
        root, this->identity_matrix, gfx::PointF(), gfx::Size(200, 200));
    LayerImpl* layer = this->CreateDrawingLayer(
        surface, translate, gfx::PointF(), gfx::Size(200, 200), false);
    TestContentLayerImpl* outside_layer = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::PointF(), gfx::Size(200, 200), false);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 200, 200));
    this->VisitLayer(outside_layer, &occlusion);
    this->EnterLayer(layer, &occlusion);

    // No occlusion, is not occluded.
    occlusion.set_occlusion_from_outside_target(SimpleEnclosedRegion());
    occlusion.set_occlusion_from_inside_target(SimpleEnclosedRegion());
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(100, 100)));

    // Partial occlusion from outside, is not occluded.
    occlusion.set_occlusion_from_outside_target(
        SimpleEnclosedRegion(50, 50, 100, 100));
    occlusion.set_occlusion_from_inside_target(SimpleEnclosedRegion());
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(0, 0, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(90, 30, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(40, 0, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(40, 80, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(0, 0, 80, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(90, 80, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(0, 80, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(90, 0, 100, 100)));

    // Full occlusion from outside, is occluded.
    EXPECT_TRUE(occlusion.OccludedLayer(layer, gfx::Rect(40, 30, 100, 100)));
    EXPECT_TRUE(occlusion.OccludedLayer(layer, gfx::Rect(40, 30, 10, 10)));
    EXPECT_TRUE(occlusion.OccludedLayer(layer, gfx::Rect(130, 120, 10, 10)));
    EXPECT_TRUE(occlusion.OccludedLayer(layer, gfx::Rect(80, 70, 50, 50)));

    // Partial occlusion from inside, is not occluded.
    occlusion.set_occlusion_from_outside_target(SimpleEnclosedRegion());
    occlusion.set_occlusion_from_inside_target(
        SimpleEnclosedRegion(50, 50, 100, 100));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(0, 0, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(90, 30, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(40, 0, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(40, 80, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(0, 0, 80, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(90, 80, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(0, 80, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(90, 0, 100, 100)));

    // Full occlusion from inside, is occluded.
    EXPECT_TRUE(occlusion.OccludedLayer(layer, gfx::Rect(40, 30, 100, 100)));
    EXPECT_TRUE(occlusion.OccludedLayer(layer, gfx::Rect(40, 30, 10, 10)));
    EXPECT_TRUE(occlusion.OccludedLayer(layer, gfx::Rect(130, 120, 10, 10)));
    EXPECT_TRUE(occlusion.OccludedLayer(layer, gfx::Rect(80, 70, 50, 50)));

    // Partial occlusion from both, is not occluded.
    occlusion.set_occlusion_from_outside_target(
        SimpleEnclosedRegion(50, 50, 100, 50));
    occlusion.set_occlusion_from_inside_target(
        SimpleEnclosedRegion(50, 100, 100, 50));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(0, 0, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(90, 30, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(40, 0, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(40, 80, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(0, 0, 80, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(90, 80, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(0, 80, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(90, 0, 100, 100)));

    // Full occlusion from both, is occluded.
    EXPECT_TRUE(occlusion.OccludedLayer(layer, gfx::Rect(40, 30, 100, 100)));
    EXPECT_TRUE(occlusion.OccludedLayer(layer, gfx::Rect(40, 30, 10, 10)));
    EXPECT_TRUE(occlusion.OccludedLayer(layer, gfx::Rect(130, 120, 10, 10)));
    EXPECT_TRUE(occlusion.OccludedLayer(layer, gfx::Rect(80, 70, 50, 50)));
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestOccludedLayer)

class OcclusionTrackerTestUnoccludedLayerQuery : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestUnoccludedLayerQuery(bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform translate;
    translate.Translate(10.0, 20.0);
    TestContentLayerImpl* root = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(200, 200));
    LayerImpl* surface = this->CreateSurface(
        root, this->identity_matrix, gfx::PointF(), gfx::Size(200, 200));
    LayerImpl* layer = this->CreateDrawingLayer(
        surface, translate, gfx::PointF(), gfx::Size(200, 200), false);
    TestContentLayerImpl* outside_layer = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::PointF(), gfx::Size(200, 200), false);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 200, 200));
    this->VisitLayer(outside_layer, &occlusion);
    this->EnterLayer(layer, &occlusion);

    // No occlusion, is not occluded.
    occlusion.set_occlusion_from_outside_target(SimpleEnclosedRegion());
    occlusion.set_occlusion_from_inside_target(SimpleEnclosedRegion());
    EXPECT_EQ(gfx::Rect(100, 100),
              occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(100, 100)));

    // Partial occlusion from outside.
    occlusion.set_occlusion_from_outside_target(
        SimpleEnclosedRegion(50, 50, 100, 100));
    occlusion.set_occlusion_from_inside_target(SimpleEnclosedRegion());
    EXPECT_EQ(
        gfx::Rect(0, 0, 100, 100),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(0, 0, 100, 100)));
    EXPECT_EQ(gfx::Rect(140, 30, 50, 100),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(90, 30, 100, 100)));
    EXPECT_EQ(gfx::Rect(40, 0, 100, 30),
              occlusion.UnoccludedLayerContentRect(layer,
                                                   gfx::Rect(40, 0, 100, 100)));
    EXPECT_EQ(gfx::Rect(40, 130, 100, 50),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(40, 80, 100, 100)));
    EXPECT_EQ(
        gfx::Rect(0, 0, 80, 100),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(0, 0, 80, 100)));
    EXPECT_EQ(gfx::Rect(90, 80, 100, 100),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(90, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(0, 80, 100, 100),
              occlusion.UnoccludedLayerContentRect(layer,
                                                   gfx::Rect(0, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(90, 0, 100, 100),
              occlusion.UnoccludedLayerContentRect(layer,
                                                   gfx::Rect(90, 0, 100, 100)));

    // Full occlusion from outside, is occluded.
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(40, 30, 100, 100)));
    EXPECT_EQ(
        gfx::Rect(),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(40, 30, 10, 10)));
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(130, 120, 10, 10)));
    EXPECT_EQ(
        gfx::Rect(),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(80, 70, 50, 50)));

    // Partial occlusion from inside, is not occluded.
    occlusion.set_occlusion_from_outside_target(SimpleEnclosedRegion());
    occlusion.set_occlusion_from_inside_target(
        SimpleEnclosedRegion(50, 50, 100, 100));
    EXPECT_EQ(
        gfx::Rect(0, 0, 100, 100),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(0, 0, 100, 100)));
    EXPECT_EQ(gfx::Rect(140, 30, 50, 100),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(90, 30, 100, 100)));
    EXPECT_EQ(gfx::Rect(40, 0, 100, 30),
              occlusion.UnoccludedLayerContentRect(layer,
                                                   gfx::Rect(40, 0, 100, 100)));
    EXPECT_EQ(gfx::Rect(40, 130, 100, 50),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(40, 80, 100, 100)));
    EXPECT_EQ(
        gfx::Rect(0, 0, 80, 100),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(0, 0, 80, 100)));
    EXPECT_EQ(gfx::Rect(90, 80, 100, 100),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(90, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(0, 80, 100, 100),
              occlusion.UnoccludedLayerContentRect(layer,
                                                   gfx::Rect(0, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(90, 0, 100, 100),
              occlusion.UnoccludedLayerContentRect(layer,
                                                   gfx::Rect(90, 0, 100, 100)));

    // Full occlusion from inside, is occluded.
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(40, 30, 100, 100)));
    EXPECT_EQ(
        gfx::Rect(),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(40, 30, 10, 10)));
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(130, 120, 10, 10)));
    EXPECT_EQ(
        gfx::Rect(),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(80, 70, 50, 50)));

    // Partial occlusion from both, is not occluded.
    occlusion.set_occlusion_from_outside_target(
        SimpleEnclosedRegion(50, 50, 100, 50));
    occlusion.set_occlusion_from_inside_target(
        SimpleEnclosedRegion(50, 100, 100, 50));
    EXPECT_EQ(
        gfx::Rect(0, 0, 100, 100),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(0, 0, 100, 100)));
    // This could be (140, 30, 50, 100). But because we do a lossy subtract,
    // it's larger.
    EXPECT_EQ(gfx::Rect(90, 30, 100, 100),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(90, 30, 100, 100)));
    EXPECT_EQ(gfx::Rect(40, 0, 100, 30),
              occlusion.UnoccludedLayerContentRect(layer,
                                                   gfx::Rect(40, 0, 100, 100)));
    EXPECT_EQ(gfx::Rect(40, 130, 100, 50),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(40, 80, 100, 100)));
    EXPECT_EQ(
        gfx::Rect(0, 0, 80, 100),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(0, 0, 80, 100)));
    EXPECT_EQ(gfx::Rect(90, 80, 100, 100),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(90, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(0, 80, 100, 100),
              occlusion.UnoccludedLayerContentRect(layer,
                                                   gfx::Rect(0, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(90, 0, 100, 100),
              occlusion.UnoccludedLayerContentRect(layer,
                                                   gfx::Rect(90, 0, 100, 100)));

    // Full occlusion from both, is occluded.
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(40, 30, 100, 100)));
    EXPECT_EQ(
        gfx::Rect(),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(40, 30, 10, 10)));
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedLayerContentRect(
                  layer, gfx::Rect(130, 120, 10, 10)));
    EXPECT_EQ(
        gfx::Rect(),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(80, 70, 50, 50)));
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestUnoccludedLayerQuery)

class OcclusionTrackerTestUnoccludedSurfaceQuery : public OcclusionTrackerTest {
 protected:
  explicit OcclusionTrackerTestUnoccludedSurfaceQuery(bool opaque_layers)
      : OcclusionTrackerTest(opaque_layers) {}
  void RunMyTest() override {
    gfx::Transform translate;
    translate.Translate(10.0, 20.0);
    TestContentLayerImpl* root = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(200, 200));
    LayerImpl* surface = this->CreateSurface(root, translate, gfx::PointF(),
                                             gfx::Size(200, 200));
    LayerImpl* layer =
        this->CreateDrawingLayer(surface, this->identity_matrix, gfx::PointF(),
                                 gfx::Size(200, 200), false);
    TestContentLayerImpl* outside_layer = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::PointF(), gfx::Size(200, 200), false);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip occlusion(gfx::Rect(0, 0, 200, 200));
    this->VisitLayer(outside_layer, &occlusion);
    this->VisitLayer(layer, &occlusion);
    this->EnterContributingSurface(surface, &occlusion);

    // No occlusion, is not occluded.
    occlusion.set_occlusion_on_contributing_surface_from_outside_target(
        SimpleEnclosedRegion());
    occlusion.set_occlusion_on_contributing_surface_from_inside_target(
        SimpleEnclosedRegion());
    EXPECT_EQ(
        gfx::Rect(100, 100),
        occlusion.UnoccludedSurfaceContentRect(surface, gfx::Rect(100, 100)));

    // Partial occlusion from outside.
    occlusion.set_occlusion_on_contributing_surface_from_outside_target(
        SimpleEnclosedRegion(50, 50, 100, 100));
    occlusion.set_occlusion_on_contributing_surface_from_inside_target(
        SimpleEnclosedRegion());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(0, 0, 100, 100)));
    EXPECT_EQ(gfx::Rect(140, 30, 50, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(90, 30, 100, 100)));
    EXPECT_EQ(gfx::Rect(40, 0, 100, 30),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(40, 0, 100, 100)));
    EXPECT_EQ(gfx::Rect(40, 130, 100, 50),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(40, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(0, 0, 80, 100),
              occlusion.UnoccludedSurfaceContentRect(surface,
                                                     gfx::Rect(0, 0, 80, 100)));
    EXPECT_EQ(gfx::Rect(90, 80, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(90, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(0, 80, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(0, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(90, 0, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(90, 0, 100, 100)));

    // Full occlusion from outside, is occluded.
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(40, 30, 100, 100)));
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(40, 30, 10, 10)));
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(130, 120, 10, 10)));
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(80, 70, 50, 50)));

    // Partial occlusion from inside, is not occluded.
    occlusion.set_occlusion_on_contributing_surface_from_outside_target(
        SimpleEnclosedRegion());
    occlusion.set_occlusion_on_contributing_surface_from_inside_target(
        SimpleEnclosedRegion(50, 50, 100, 100));
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(0, 0, 100, 100)));
    EXPECT_EQ(gfx::Rect(140, 30, 50, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(90, 30, 100, 100)));
    EXPECT_EQ(gfx::Rect(40, 0, 100, 30),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(40, 0, 100, 100)));
    EXPECT_EQ(gfx::Rect(40, 130, 100, 50),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(40, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(0, 0, 80, 100),
              occlusion.UnoccludedSurfaceContentRect(surface,
                                                     gfx::Rect(0, 0, 80, 100)));
    EXPECT_EQ(gfx::Rect(90, 80, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(90, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(0, 80, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(0, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(90, 0, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(90, 0, 100, 100)));

    // Full occlusion from inside, is occluded.
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(40, 30, 100, 100)));
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(40, 30, 10, 10)));
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(130, 120, 10, 10)));
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(80, 70, 50, 50)));

    // Partial occlusion from both, is not occluded.
    occlusion.set_occlusion_on_contributing_surface_from_outside_target(
        SimpleEnclosedRegion(50, 50, 100, 50));
    occlusion.set_occlusion_on_contributing_surface_from_inside_target(
        SimpleEnclosedRegion(50, 100, 100, 50));
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(0, 0, 100, 100)));
    // This could be (140, 30, 50, 100). But because we do a lossy subtract,
    // it's larger.
    EXPECT_EQ(gfx::Rect(90, 30, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(90, 30, 100, 100)));
    EXPECT_EQ(gfx::Rect(40, 0, 100, 30),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(40, 0, 100, 100)));
    EXPECT_EQ(gfx::Rect(40, 130, 100, 50),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(40, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(0, 0, 80, 100),
              occlusion.UnoccludedSurfaceContentRect(surface,
                                                     gfx::Rect(0, 0, 80, 100)));
    EXPECT_EQ(gfx::Rect(90, 80, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(90, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(0, 80, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(0, 80, 100, 100)));
    EXPECT_EQ(gfx::Rect(90, 0, 100, 100),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(90, 0, 100, 100)));

    // Full occlusion from both, is occluded.
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(40, 30, 100, 100)));
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(40, 30, 10, 10)));
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(130, 120, 10, 10)));
    EXPECT_EQ(gfx::Rect(),
              occlusion.UnoccludedSurfaceContentRect(
                  surface, gfx::Rect(80, 70, 50, 50)));
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestUnoccludedSurfaceQuery)

}  // namespace
}  // namespace cc
