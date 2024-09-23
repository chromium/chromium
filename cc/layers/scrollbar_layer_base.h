// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SCROLLBAR_LAYER_BASE_H_
#define CC_LAYERS_SCROLLBAR_LAYER_BASE_H_

#include "cc/cc_export.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/layer.h"

namespace cc {

class CC_EXPORT ScrollbarLayerBase : public Layer {
 public:
  static scoped_refptr<ScrollbarLayerBase> CreateOrReuse(
      scoped_refptr<Scrollbar>,
      ScrollbarLayerBase* existing_layer);

  void SetScrollElementId(ElementId element_id);
  ElementId scroll_element_id() const { return scroll_element_id_.Read(*this); }

  ScrollbarOrientation orientation() const { return orientation_; }
  bool is_left_side_vertical_scrollbar() const {
    return is_left_side_vertical_scrollbar_;
  }
  bool has_find_in_page_tickmarks() const {
    return has_find_in_page_tickmarks_.Read(*this);
  }
  bool SetHasFindInPageTickmarks(bool has_find_in_page_tickmarks);

  void PushPropertiesTo(LayerImpl* layer,
                        const CommitState& commit_state,
                        const ThreadUnsafeCommitState& unsafe_state) override;

  enum ScrollbarLayerType {
    kSolidColor,
    kPainted,
    kNinePatchThumb,
  };
  virtual ScrollbarLayerType GetScrollbarLayerType() const = 0;

 protected:
  ScrollbarLayerBase(ScrollbarOrientation orientation,
                     bool is_left_side_vertical_scrollbar);
  ~ScrollbarLayerBase() override;

 private:
  bool IsScrollbarLayerForTesting() const final;

  const ScrollbarOrientation orientation_;
  const bool is_left_side_vertical_scrollbar_;
  ProtectedSequenceReadable<ElementId> scroll_element_id_;
  ProtectedSequenceReadable<bool> has_find_in_page_tickmarks_;
};

}  // namespace cc

#endif  // CC_LAYERS_SCROLLBAR_LAYER_BASE_H_
