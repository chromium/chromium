// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "ash/app_list/app_list_util.h"
#include "ash/app_list/test/app_list_test_model.h"
#include "ash/app_list/test/app_list_test_view_delegate.h"
#include "ash/app_list/views/app_list_view.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views_content_client/views_content_client.h"

namespace ash {

// Number of dummy apps to populate in the app list.
const int kInitialItems = 20;

// Extends the test AppListViewDelegate to quit the run loop when the launcher
// window is closed, and to close the window if it is simply dismissed.
class DemoAppListViewDelegate : public test::AppListTestViewDelegate {
 public:
  DemoAppListViewDelegate() : view_(nullptr) {}
  ~DemoAppListViewDelegate() override {}

  AppListView* InitView(gfx::NativeWindow window_context);

  // Overridden from AppListViewDelegate:
  void DismissAppList() override;
  void ViewClosing() override;

 private:
  AppListView* view_;  // Weak. Owns this.

  DISALLOW_COPY_AND_ASSIGN(DemoAppListViewDelegate);
};

AppListView* DemoAppListViewDelegate::InitView(
    gfx::NativeWindow window_context) {
  // On Ash, the app list is placed into an aura::Window container. For the demo
  // use the root window context as the parent. This only works on Aura since an
  // aura::Window is also a NativeView.
  gfx::NativeView container = window_context;

  view_ = new AppListView(this);
  view_->InitView(
      /*is_tablet_mode=*/false, container,
      base::BindRepeating(&UpdateActivationForAppListView, view_,
                          /*is_tablet_mode=*/false));
  view_->Show(false /*is_side_shelf*/, false /*is_tablet_mode*/);

  // Populate some apps.
  GetTestModel()->PopulateApps(kInitialItems);
  AppListItemList* item_list = GetTestModel()->top_level_item_list();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::Image test_image = rb.GetImageNamed(IDR_DEFAULT_FAVICON_32);
  for (size_t i = 0; i < item_list->item_count(); ++i) {
    AppListItem* item = item_list->item_at(i);
    // Alternate images with shadows and images without.
    item->SetIcon(ash::AppListConfigType::kShared, *test_image.ToImageSkia());
  }
  return view_;
}

void DemoAppListViewDelegate::DismissAppList() {
  view_->GetWidget()->Close();
}

void DemoAppListViewDelegate::ViewClosing() {
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

void ShowAppList(content::BrowserContext* browser_context,
                 gfx::NativeWindow window_context) {
  DemoAppListViewDelegate* delegate = new DemoAppListViewDelegate;
  AppListView* view = delegate->InitView(window_context);
  view->GetWidget()->Show();
  view->GetWidget()->Activate();
}

}  // namespace ash

int main(int argc, const char** argv) {
  ui::ViewsContentClient views_content_client(argc, argv);
  views_content_client.set_on_pre_main_message_loop_run_callback(
      base::BindOnce(&ash::ShowAppList));
  return views_content_client.RunMain();
}
