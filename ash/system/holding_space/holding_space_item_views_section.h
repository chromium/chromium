// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEWS_SECTION_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEWS_SECTION_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_section.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ui {
class CallbackLayerAnimationObserver;
class LayerAnimationObserver;
}  // namespace ui

namespace views {
class ScrollView;
}  // namespace views

namespace ash {

class HoldingSpaceItemView;
class HoldingSpaceViewDelegate;

// A section of holding space item views in a `HoldingSpaceTrayChildBubble`.
class ASH_EXPORT HoldingSpaceItemViewsSection : public views::View {
  METADATA_HEADER(HoldingSpaceItemViewsSection, views::View)

 public:
  HoldingSpaceItemViewsSection(HoldingSpaceViewDelegate* delegate,
                               HoldingSpaceSectionId section_id);
  HoldingSpaceItemViewsSection(const HoldingSpaceItemViewsSection& other) =
      delete;
  HoldingSpaceItemViewsSection& operator=(
      const HoldingSpaceItemViewsSection& other) = delete;
  ~HoldingSpaceItemViewsSection() override;

  // Initializes the section.
  void Init();

  // Resets the section. Called when the tray bubble starts closing to ensure
  // that no new items are created while the bubble widget is being
  // asynchronously closed.
  void Reset();

  // Returns all holding space item views in the section. Views are returned in
  // top-to-bottom, left-to-right order (or mirrored for RTL).
  std::vector<HoldingSpaceItemView*> GetHoldingSpaceItemViews();

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;
  void PreferredSizeChanged() override;
  void ViewHierarchyChanged(const views::ViewHierarchyChangedDetails&) override;

  // `HoldingSpaceModelObserver` events forwarded from the parent
  // `HoldingSpaceTrayChildBubble`. Note that events may be withheld from this
  // view if, for example, its parent is animating out.
  void OnHoldingSpaceItemsAdded(const std::vector<const HoldingSpaceItem*>&);
  void OnHoldingSpaceItemsRemoved(const std::vector<const HoldingSpaceItem*>&);
  void OnHoldingSpaceItemInitialized(const HoldingSpaceItem* item);

  // Removes all holding space item views from this section. This method is
  // expected to only be called:
  // * from the parent `HoldingSpaceTrayChildBubble` when this view is hidden.
  // * internally after having animated out the `container_` just prior to
  //   swapping in new contents.
  void RemoveAllHoldingSpaceItemViews();

  // Returns whether this section has a placeholder to show in lieu of item
  // views when the model contains no initialized items of supported types.
  bool has_placeholder() const { return !!placeholder_; }

  // Returns the types of holding space items supported by this section.
  const std::set<HoldingSpaceItem::Type>& supported_types() const {
    return section_->supported_types;
  }

 protected:
  // Invoked to create the `header_` for this section.
  virtual std::unique_ptr<views::View> CreateHeader() = 0;

  // Invoked to create the `container_` for this section which parents its
  // holding space item views.
  virtual std::unique_ptr<views::View> CreateContainer() = 0;

  // Invoked to create the view for the specified holding space `item`. Note
  // that the created view will be parented by `container_`.
  virtual std::unique_ptr<HoldingSpaceItemView> CreateView(
      const HoldingSpaceItem* item) = 0;

  // Invoked to create the `placeholder_` for this section which shows when
  // `container_` is empty. The `placeholder_` can be destroyed via call to
  // `DestroyPlaceholder()` if it is no longer needed to exist.
  virtual std::unique_ptr<views::View> CreatePlaceholder();

  // Invoked to destroy `placeholder_`.
  void DestroyPlaceholder();

  // Whether to display this section's contents: either its `container_` or its
  // `placeholder_` as applicable. Sections that have no concept of expanded
  // state are always treated as expanded.
  virtual bool IsExpanded();

  // Updates the section's views based on changes to the expanded state.
  void OnExpandedChanged();

  HoldingSpaceViewDelegate* delegate() { return delegate_; }

 private:
  enum AnimationState : uint32_t {
    kNotAnimating = 0,
    kAnimatingIn = 1 << 1,
    kAnimatingOut = 1 << 2,
  };

  // Invoke to start animating in the contents of this section. No-ops if
  // animate in is already in progress.
  void MaybeAnimateIn();

  // Invoke to start animating out the contents of this section. No-ops if
  // animate out is already in progress.
  void MaybeAnimateOut();

  // Invoked to animate in the contents of this section. Any created animation
  // sequences must be observed by `observer`.
  void AnimateIn(ui::LayerAnimationObserver* observer);

  // Invoked to animate out the contents of this section. Any created animation
  // sequences must be observed by `observer`.
  void AnimateOut(ui::LayerAnimationObserver* observer);

  // Invoked when an animate in/out of the contents of this section has been
  // completed. Note that the provided observer will be deleted after returning.
  void OnAnimateInCompleted(const ui::CallbackLayerAnimationObserver&);
  void OnAnimateOutCompleted(const ui::CallbackLayerAnimationObserver&);

  const raw_ptr<HoldingSpaceViewDelegate, DanglingUntriaged> delegate_;
  const raw_ptr<const HoldingSpaceSection> section_;

  // Owned by view hierarchy.
  raw_ptr<views::View> header_ = nullptr;
  raw_ptr<views::View> container_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> placeholder_ = nullptr;
  raw_ptr<views::ScrollView> scroll_view_ = nullptr;
  std::map<std::string, raw_ptr<HoldingSpaceItemView, CtnExperimental>>
      views_by_item_id_;

  // Bit flag representation of current `AnimationState`. Note that it is
  // briefly possible to be both `kAnimatingIn` and `kAnimatingOut` when one
  // animation is preempting another.
  uint32_t animation_state_ = AnimationState::kNotAnimating;

  // Whether or not animations are disabled. Animations are only disabled during
  // initialization as holding space child bubbles are animated in instead.
  bool disable_animations_ = false;

  // Whether or not `PreferredSizeChanged()` is allowed to propagate up the
  // view hierarchy. This is disabled during batch child additions, removals,
  // and visibility change operations to reduce the number of layout events.
  bool disable_preferred_size_changed_ = false;

  base::WeakPtrFactory<HoldingSpaceItemViewsSection> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEWS_SECTION_H_
