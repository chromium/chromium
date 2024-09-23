// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRI_VIEW_H_
#define ASH_SYSTEM_TRAY_TRI_VIEW_H_

#include <memory>
#include <utility>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

namespace views {
class Border;
class BoxLayout;
class LayoutManager;
}  // namespace views

namespace ash {
class SizeRangeLayout;

// A View which has 3 child containers (START, CENTER, END) which can be
// arranged vertically or horizontally. The child containers can have minimum
// and/or maximum preferred size defined as well as a flex weight that is used
// to distribute excess space across the main axis, i.e. flexible width for the
// horizontal orientation. By default all the containers have a flex weight of
// 0, meaning no flexibility, and no minimum or maximum size.
//
// Child views should not be added to |this| directly via View::AddChildView()
// or View::AddChildViewAt() and will fail a DCHECK() if attempted.
//
// Views added to the containers are laid out as per the LayoutManager that has
// been installed on that container. By default a BoxLayout manager is installed
// on each container with the same orientation as |this| has been created with.
// The default BoxLayout will use a center alignment for both the main axis and
// cross axis alignment.
class ASH_EXPORT TriView : public views::View {
  METADATA_HEADER(TriView, views::View)

 public:
  enum class Orientation {
    HORIZONTAL,
    VERTICAL,
  };

  // The different containers that child Views can be added to.
  enum class Container { START = 0, CENTER = 1, END = 2 };

  // Constructs a layout with horizontal orientation and 0 padding between
  // containers.
  TriView();

  // Creates |this| with a Horizontal orientation and the specified padding
  // between containers.
  //
  // TODO(bruthig): The |padding_between_containers| can only be set on
  // BoxLayouts during construction. Investigate whether this can be a mutable
  // property of BoxLayouts and if so consider dropping it as a constructor
  // parameter here.
  explicit TriView(int padding_between_containers);

  // Creates |this| with the specified orientation and 0 padding between
  // containers.
  explicit TriView(Orientation orientation);

  // Creates this with the specified |orientation| and
  // |padding_between_containers|.
  TriView(Orientation orientation, int padding_between_containers);

  TriView(const TriView&) = delete;
  TriView& operator=(const TriView&) = delete;

  ~TriView() override;

  views::BoxLayout* box_layout() { return box_layout_; }

  // Set the minimum height for all containers to |height|.
  void SetMinHeight(int height);

  // Set the minimum size for the given |container|.
  void SetMinSize(Container container, const gfx::Size& size);

  // Get the minimum size for the given |container|.
  gfx::Size GetMinSize(Container container);

  // Set the maximum size for the given |container|.
  void SetMaxSize(Container container, const gfx::Size& size);

  // Adds the child `view` to the specified `container`. Returns a raw pointer
  // to the view.
  template <typename T>
  T* AddView(Container container, std::unique_ptr<T> view) {
    return GetContainer(container)->AddChildView(std::move(view));
  }

  // Adds the child `view` to the specified `container` at the child index.
  // Returns a raw pointer to the view.
  template <typename T>
  T* AddViewAt(Container container, std::unique_ptr<T> view, int index) {
    return GetContainer(container)->AddChildViewAt(std::move(view), index);
  }

  // Adds the child `view` to the specified `container`. Takes ownership of the
  // view. Prefer the unique_ptr version above when possible.
  void AddView(Container container, views::View* view);

  // Adds the child `view` to the specified `container` at the child index.
  // Takes ownership of the view. Prefer the unique_ptr version above when
  // possible.
  void AddViewAt(Container container, views::View* view, int index);

  // During layout the |insets| are applied to the host views entire space
  // before allocating the remaining space to the container views.
  void SetInsets(const gfx::Insets& insets);

  // Sets the border for the given |container|.
  void SetContainerBorder(Container container,
                          std::unique_ptr<views::Border> border);

  // Sets whether the |container| is visible. During a layout the space will be
  // allocated to the visible containers only. i.e. non-visible containers will
  // not be allocated any space.
  // Note: This will cause a relayout.
  void SetContainerVisible(Container container, bool visible);

  // Sets the flex weight for the given |container|. Using the preferred size as
  // the basis, free space along the main axis is distributed to views in the
  // ratio of their flex weights. Similarly, if the views will overflow the
  // parent, space is subtracted in these ratios.
  //
  // A flex of 0 means this view is not resized. Flex values must not be
  // negative.
  //
  // Note that non-zero flex values will take precedence over size constraints.
  // i.e. even if |container| has a max size set the space allocated during
  // layout may be larger if |flex| > 0 and similar for min size constraints.
  void SetFlexForContainer(Container container, int flex);

  // Sets the |layout_manager| used by the given |container|.
  void SetContainerLayout(Container container,
                          std::unique_ptr<views::LayoutManager> layout_manager);

 protected:
  // View:
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;
  gfx::Rect GetAnchorBoundsInScreen() const override;

 private:
  friend class TriViewTest;

  // Returns the View for the given |container|.
  views::View* GetContainer(Container container);

  // Returns the layout manager for the given |container|.
  SizeRangeLayout* GetLayoutManager(Container container);

  // Type spcific layout manager installed on |this|. Responsible for laying out
  // the container Views.
  raw_ptr<views::BoxLayout> box_layout_ = nullptr;

  raw_ptr<SizeRangeLayout> start_container_layout_manager_ = nullptr;
  raw_ptr<SizeRangeLayout> center_container_layout_manager_ = nullptr;
  raw_ptr<SizeRangeLayout> end_container_layout_manager_ = nullptr;

  // In order to detect direct manipulation of child views the
  // ViewHierarchyChanged() event override fails on a DCHECK. However, we need
  // to manipulate the child views during construction/destruction so this flag
  // is used to disable the DCHECK during construction/destruction.
  bool enable_hierarchy_changed_dcheck_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRI_VIEW_H_
