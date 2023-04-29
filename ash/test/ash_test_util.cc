// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_util.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot_aura.h"

namespace ash {

namespace {
void SnapshotCallback(base::RunLoop* run_loop,
                      gfx::Image* ret_image,
                      gfx::Image image) {
  *ret_image = image;
  run_loop->Quit();
}
}  // namespace

bool TakePrimaryDisplayScreenshotAndSave(const base::FilePath& file_path) {
  // Return false if the file extension is not "png".
  if (file_path.Extension() != ".png")
    return false;

  // Return false if `file_path`'s directory does not exist.
  const base::FilePath directory_name = file_path.DirName();
  if (!base::PathExists(directory_name))
    return false;

  base::RunLoop run_loop;
  gfx::Image image;
  ui::GrabWindowSnapshotAsyncAura(
      Shell::Get()->GetPrimaryRootWindow(),
      Shell::Get()->GetPrimaryRootWindow()->bounds(),
      base::BindOnce(&SnapshotCallback, &run_loop, &image));
  run_loop.Run();
  auto data = image.As1xPNGBytes();
  DCHECK_GT(data->size(), 0u);
  return base::WriteFile(file_path, *data);
}

void GiveItSomeTimeForDebugging(base::TimeDelta time_duration) {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), time_duration);
  run_loop.Run();
}

bool IsSystemTrayForRootWindowVisible(size_t root_window_index) {
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  RootWindowController* controller =
      RootWindowController::ForWindow(root_windows[root_window_index]);
  return controller->GetStatusAreaWidget()->unified_system_tray()->GetVisible();
}

gfx::ImageSkia CreateSolidColorTestImage(const gfx::Size& image_size,
                                         SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(image_size.width(), image_size.height());
  bitmap.eraseColor(color);
  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  return image;
}

void DecorateWindow(aura::Window* window,
                    const std::u16string& title,
                    SkColor color) {
  auto* widget = views::Widget::GetWidgetForNativeWindow(window);
  DCHECK(widget);
  widget->client_view()->AddChildView(
      views::Builder<views::View>()
          .SetBackground(views::CreateRoundedRectBackground(color, 4.f))
          .Build());

  // Add a title and an app icon so that the header is fully stocked.
  window->SetTitle(title);
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(SK_ColorCYAN);
  window->SetProperty(aura::client::kAppIconKey,
                      gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
}

}  // namespace ash
