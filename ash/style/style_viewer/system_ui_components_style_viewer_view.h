// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_STYLE_VIEWER_SYSTEM_UI_COMPONENTS_STYLE_VIEWER_VIEW_H_
#define ASH_STYLE_STYLE_VIEWER_SYSTEM_UI_COMPONENTS_STYLE_VIEWER_VIEW_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class ScrollView;
}  // namespace views

namespace ash {

class SystemUIComponentsGridView;

// SystemUIComponentsStyleViewerView is the client view of the system components
// style viewer. It has two parts: (1) the menu scroll view contains a list of
// component buttons which can toggle the component instances, (2) the component
// instances scroll view lists the instances of current selected component. The
// layout is shown below:
// +----------------------+---------------------------------------+
// |  Menu Scroll View    |    Component Instances Scroll View    |
// | +------------------+ |                                       |
// | | Component Button | |                                       |
// | +------------------+ |                                       |
// | | Component Button | |                                       |
// | +------------------+ |                                       |
// |                      |                                       |
// +----------------------+---------------------------------------+
class SystemUIComponentsStyleViewerView : public views::WidgetDelegateView,
                                          public views::WidgetObserver {
  METADATA_HEADER(SystemUIComponentsStyleViewerView, views::WidgetDelegateView)

 public:
  // A view factory of `SystemUIComponentsGridView` that shows the UI component
  // instances in a m x n grids.
  using ComponentsGridViewFactory =
      base::RepeatingCallback<std::unique_ptr<SystemUIComponentsGridView>(
          void)>;

  SystemUIComponentsStyleViewerView();
  SystemUIComponentsStyleViewerView(const SystemUIComponentsStyleViewerView&) =
      delete;
  SystemUIComponentsStyleViewerView& operator=(
      const SystemUIComponentsStyleViewerView&) = delete;
  ~SystemUIComponentsStyleViewerView() override;

  // Creates a widget using the view as delegate and contents.
  static void CreateAndShowWidget();

  // Adds a new component with component name and grid view factory.
  void AddComponent(const std::u16string& name,
                    ComponentsGridViewFactory grid_view_factory);
  // Shows the instances of the UI component indicated by the given name.
  void ShowComponentInstances(const std::u16string& name);

  // views::WidgetDelegateView:
  void Layout() override;
  std::u16string GetWindowTitle() const override;

  // views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override;

 private:
  // The button which is used to toggle the instances of corresponding
  // component.
  class ComponentButton;

  // The scroll views.
  raw_ptr<views::ScrollView, ExperimentalAsh> menu_scroll_view_;
  raw_ptr<views::ScrollView, ExperimentalAsh> component_instances_scroll_view_;
  // The contents of the scroll views.
  raw_ptr<views::View, ExperimentalAsh> menu_contents_view_;
  raw_ptr<views::View, ExperimentalAsh> components_grid_view_;
  // Buttons used to toggle the component instances.
  std::vector<ComponentButton*> buttons_;
  // Factories of `SystemUIComponentsGridView` for different
  // UI components.
  std::map<std::u16string, ComponentsGridViewFactory>
      components_grid_view_factories_;
};

}  // namespace ash

#endif  // ASH_STYLE_STYLE_VIEWER_SYSTEM_UI_COMPONENTS_STYLE_VIEWER_VIEW_H_
