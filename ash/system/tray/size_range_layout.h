// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SIZE_RANGE_LAYOUT_H_
#define ASH_SYSTEM_TRAY_SIZE_RANGE_LAYOUT_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/layout_manager.h"

namespace views {
class View;
}  // namespace views

namespace ash {

// A LayoutManager adapter that allows clients to specify a minimum and/or a
// maximum preferred size. The actual layout will be delegated to the
// LayoutManager owned by this. i.e. |this| can be used to override the
// preferred size returned by a View.
//
// By default the SizeRangeLayout is configured to own a FillLayout but this can
// be overridden with SetLayoutManager().
//
// Example use case :
//
//  Suppose you wanted a Label to take up a specific size of (50, 50) even
//  though the label's preferred size was (25, 25).
//
// Example code:
//
//  Label* label = new Label(kSomeDummyText);
//  View* container = new View();
//  container->AddChildView(label);
//  SizeRangeLayout* layout = new SizeRangeLayout();
//  layout->SetSize(gfx::Size(50, 50));
//  container->SetLayoutManager(layout);
//
class ASH_EXPORT SizeRangeLayout : public views::LayoutManager {
 public:
  // Create a layout with no minimum or maximum preferred size.
  SizeRangeLayout();

  // Create a layout using the given size set as the minimum and maximum sizes.
  explicit SizeRangeLayout(const gfx::Size& size);

  // Create a layout with the given minimum and maximum preferred sizes. If
  // |max_size| is smaller than |min_size| then |min_size| will be set to the
  // smaller |max_size| value.
  SizeRangeLayout(const gfx::Size& min_size, const gfx::Size& max_size);

  SizeRangeLayout(const SizeRangeLayout&) = delete;
  SizeRangeLayout& operator=(const SizeRangeLayout&) = delete;

  ~SizeRangeLayout() override;

  // The absolute minimum possible width/height. Use this with SetMinSize() to
  // effectively unset the minimum preferred size.
  static const int kAbsoluteMinSize;

  // Tthe absolute maximum possible width/height. Use this with SetMaxSize() to
  // effectively unset the maximum preferred size.
  static const int kAbsoluteMaxSize;

  // Sets both the minimum and maximum preferred size.
  void SetSize(const gfx::Size& size);
  void SetSize(int width, int height);

  // Set the minimum preferred size that GetPreferredSize() will round up to. If
  // |size| is larger than the current |max_size_| then |max_size_| will set to
  // |size| as well.
  void SetMinSize(const gfx::Size& size);
  void SetMinSize(int width, int height);

  gfx::Size min_size() const { return min_size_; }

  // Set the minimum preferred size that GetPreferredSize() will round down to.
  // If |size| is smaller than the current |min_size_| then |min_size_| will set
  // to |size| as well.
  void SetMaxSize(const gfx::Size& size);

  // Sets the layout manager that actually performs the layout once the bounds
  // have been defined.
  void SetLayoutManager(std::unique_ptr<LayoutManager> layout_manager);

  // LayoutManager:
  void Installed(views::View* host) override;
  void Layout(views::View* host) override;
  gfx::Size GetPreferredSize(const views::View* host) const override;
  int GetPreferredHeightForWidth(const views::View* host,
                                 int width) const override;
  void ViewAdded(views::View* host, views::View* view) override;
  void ViewRemoved(views::View* host, views::View* view) override;

 private:
  friend class SizeRangeLayoutTest;

  // Clamps |size| to be within the minimum and maximum preferred sizes.
  void ClampSizeToRange(gfx::Size* size) const;

  // The host View that this has been installed on.
  views::View* host_ = nullptr;

  // The layout manager that actually performs the layout.
  std::unique_ptr<views::LayoutManager> layout_manager_;

  // The minimum preferred size.
  gfx::Size min_size_;

  // The maximum preferred size.
  gfx::Size max_size_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SIZE_RANGE_LAYOUT_H_
