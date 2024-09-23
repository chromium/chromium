// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_SCROLLBAR_LAYER_H_
#define CC_TEST_FAKE_SCROLLBAR_LAYER_H_

#include <stddef.h>

#include <utility>

#include "cc/layers/nine_patch_thumb_scrollbar_layer.h"
#include "cc/layers/painted_scrollbar_layer.h"
#include "cc/test/fake_scrollbar.h"

namespace cc {

struct FakeScrollbarParams {
  bool paint_during_update = true;
};

template <typename BaseLayer>
class FakeScrollbarLayer : public BaseLayer {
 public:
  int update_count() const { return update_count_; }
  void reset_update_count() { update_count_ = 0; }

  bool Update() override {
    bool updated = BaseLayer::Update();
    ++update_count_;
    return updated;
  }

  void PushPropertiesTo(LayerImpl* layer,
                        const CommitState& commit_state,
                        const ThreadUnsafeCommitState& unsafe_state) override {
    BaseLayer::PushPropertiesTo(layer, commit_state, unsafe_state);
    ++push_properties_count_;
  }

  using BaseLayer::IgnoreSetNeedsCommitForTest;

  size_t push_properties_count() const { return push_properties_count_; }
  void reset_push_properties_count() { push_properties_count_ = 0; }

  FakeScrollbar* fake_scrollbar() { return fake_scrollbar_.get(); }

 protected:
  FakeScrollbarLayer(ElementId scrolling_element_id,
                     scoped_refptr<FakeScrollbar> fake_scrollbar)
      : BaseLayer(fake_scrollbar),
        update_count_(0),
        push_properties_count_(0),
        fake_scrollbar_(std::move(fake_scrollbar)) {
    BaseLayer::SetScrollElementId(scrolling_element_id);
    BaseLayer::SetBounds(gfx::Size(1, 1));
    BaseLayer::SetIsDrawable(true);
  }

  ~FakeScrollbarLayer() override = default;

 private:
  int update_count_;
  size_t push_properties_count_;
  scoped_refptr<FakeScrollbar> fake_scrollbar_;
};

class FakePaintedScrollbarLayer
    : public FakeScrollbarLayer<PaintedScrollbarLayer> {
 public:
  explicit FakePaintedScrollbarLayer(
      ElementId scrolling_element_id,
      scoped_refptr<FakeScrollbar> fake_scrollbar = CreateScrollbar())
      : FakeScrollbarLayer(scrolling_element_id, std::move(fake_scrollbar)) {}

  static scoped_refptr<FakeScrollbar> CreateScrollbar() {
    auto scrollbar = base::MakeRefCounted<FakeScrollbar>();
    scrollbar->set_has_thumb(true);
    scrollbar->set_uses_solid_color_thumb(true);
    scrollbar->set_uses_nine_patch_track_and_buttons_resource(true);
    return scrollbar;
  }

  using PaintedScrollbarLayer::thumb_resource_id;
  using PaintedScrollbarLayer::track_and_buttons_resource_id;

 private:
  ~FakePaintedScrollbarLayer() override = default;
};

class FakeNinePatchThumbScrollbarLayer
    : public FakeScrollbarLayer<NinePatchThumbScrollbarLayer> {
 public:
  explicit FakeNinePatchThumbScrollbarLayer(
      ElementId scrolling_element_id,
      scoped_refptr<FakeScrollbar> fake_scrollbar = CreateScrollbar())
      : FakeScrollbarLayer(scrolling_element_id, std::move(fake_scrollbar)) {}

  static scoped_refptr<FakeScrollbar> CreateScrollbar() {
    auto scrollbar = base::MakeRefCounted<FakeScrollbar>();
    scrollbar->set_has_thumb(true);
    scrollbar->set_uses_nine_patch_thumb_resource(true);
    scrollbar->set_is_overlay(true);
    return scrollbar;
  }

 private:
  ~FakeNinePatchThumbScrollbarLayer() override = default;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_SCROLLBAR_LAYER_H_
