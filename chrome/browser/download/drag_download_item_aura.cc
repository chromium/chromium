// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/task/current_thread.h"
#include "build/build_config.h"
#include "chrome/browser/download/drag_download_item.h"
#include "components/download/public/common/download_item.h"
#include "net/base/mime_util.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/button_drag_utils.h"
#include "url/gurl.h"

void DragDownloadItem(const download::DownloadItem* download,
                      const gfx::Image* icon,
                      gfx::NativeView view) {
  DCHECK(download);
  DCHECK_EQ(download::DownloadItem::COMPLETE, download->GetState());

  aura::Window* root_window = view->GetRootWindow();
  if (!root_window || !aura::client::GetDragDropClient(root_window))
    return;

  // Set up our OLE machinery.
  auto data = std::make_unique<ui::OSExchangeData>();

  button_drag_utils::SetDragImage(
      GURL(), download->GetFileNameToReportUser().BaseName().LossyDisplayName(),
      icon ? icon->AsImageSkia() : gfx::ImageSkia(), nullptr, data.get());

  base::FilePath full_path = download->GetTargetFilePath();
  std::vector<ui::FileInfo> file_infos;
  file_infos.emplace_back(full_path, download->GetFileNameToReportUser());
  data->SetFilenames(file_infos);

  gfx::Point location = display::Screen::GetScreen()->GetCursorScreenPoint();

  // The following call to StartDragAndDrop() causes re-entrancy inside which
  // application tasks must be run (to support dragging downloads into
  // WebContents).
  base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;
  // TODO(varunjain): Properly determine and send DragEventSource below.
  aura::client::GetDragDropClient(root_window)
      ->StartDragAndDrop(
          std::move(data), root_window, view, location,
          ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK,
          ui::mojom::DragEventSource::kMouse);
}
