// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_VIEW_BUILDER_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_VIEW_BUILDER_H_

#include "memory"
#include "vector"

#include "base/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

namespace views {
class LayoutManager;
}  // namespace views

namespace ash {

// A class which facilitates building of a view hierarchy. It is designed to be
// used similarly to (and conjunction with) `views::Builder` but differs in that
// it uses `std::unique_ptr<ViewType>` as its underlying storage mechanism
// rather than wrapped `views::Builder<ViewType>` references. This allows it to
// interop with both builders as well as traditionally constructed views.
// NOTE: The intention is to remove this class once changes have been made at
// the views framework level to support improved child lifetime management in
// `views::Builder>. That effort is being tracked in https://crbug.com/1218115.
template <typename ViewType>
class HoldingSpaceViewBuilder {
 public:
  // Constructs an instance from a `builder`.
  template <typename BuilderType>
  HoldingSpaceViewBuilder(BuilderType& builder)
      : HoldingSpaceViewBuilder(builder.Build()) {}

  // Constructs an instance from an owned `root_view`.
  explicit HoldingSpaceViewBuilder(std::unique_ptr<ViewType> root_view)
      : owned_root_view_(std::move(root_view)),
        root_view_(owned_root_view_.get()) {
    DCHECK(owned_root_view_);
    DCHECK(root_view_);
  }

  // Constructs an instance from an unowned `root_view`.
  explicit HoldingSpaceViewBuilder(ViewType* root_view)
      : root_view_(root_view) {
    DCHECK(root_view_);
  }

  HoldingSpaceViewBuilder(const HoldingSpaceViewBuilder&) = delete;
  HoldingSpaceViewBuilder& operator=(const HoldingSpaceViewBuilder&) = delete;
  ~HoldingSpaceViewBuilder() = default;

  // Builds and returns an `owned_root_view_`.
  std::unique_ptr<ViewType> Build() {
    DCHECK(owned_root_view_);
    BuildChildren();
    return std::move(owned_root_view_);
  }

  // Builds children for a potentially unowned `root_view_`.
  void BuildChildren() {
    for (auto& child : children_)
      root_view_->AddChildView(std::move(child));
    children_.clear();
  }

  // Adds a pending child to be added to `root_view_` at `Build*()` time,
  // returning a reference to `this` as a convenience.
  template <typename BuilderType>
  HoldingSpaceViewBuilder<ViewType>& AddChild(BuilderType& builder) {
    return AddChild(builder.Build());
  }

  // Adds a pending `child` to be added to `root_view_` at `Build*()` time,
  // returning a reference to `this` as a convenience.
  HoldingSpaceViewBuilder<ViewType>& AddChild(
      std::unique_ptr<views::View> child) {
    children_.push_back(std::move(child));
    return *this;
  }

  // Adds a pending `child` to be added to `root_view_` at `Build*()` time if
  // the specified `condition` is `true`, returning a reference to `this` as a
  // convenience.
  HoldingSpaceViewBuilder<ViewType>& AddChildIf(
      bool condition,
      base::OnceCallback<std::unique_ptr<views::View>()> callback) {
    return condition ? AddChild(std::move(callback).Run()) : *this;
  }

  // Copies the address of `root_view_` to the specified `address_ptr`,
  // returning a reference to `this` as a convenience.
  HoldingSpaceViewBuilder<ViewType>& CopyAddressTo(ViewType** address_ptr) {
    *address_ptr = root_view_;
    return *this;
  }

  // Sets the `id` for `root_view_`, returning a reference to `this` as a
  // convenience.
  HoldingSpaceViewBuilder<ViewType>& SetID(int id) {
    root_view_->SetID(id);
    return *this;
  }

  // Sets the `layout_manager` for `root_view_`, returning a reference to `this`
  // as a convenience.
  HoldingSpaceViewBuilder<ViewType>& SetLayoutManager(
      std::unique_ptr<views::LayoutManager> layout_manager) {
    root_view_->SetLayoutManager(std::move(layout_manager));
    return *this;
  }

  // Sets the `preferred_size` for `root_view`, returning a reference to `this`
  // as a convenience.
  HoldingSpaceViewBuilder<ViewType>& SetPreferredSize(
      const gfx::Size& preferred_size) {
    root_view_->SetPreferredSize(preferred_size);
    return *this;
  }

 private:
  std::unique_ptr<ViewType> owned_root_view_;
  ViewType* const root_view_;
  std::vector<std::unique_ptr<views::View>> children_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_VIEW_BUILDER_H_
