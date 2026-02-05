// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/drag_download_item.h"

#import <Cocoa/Cocoa.h>

#include "base/apple/foundation_util.h"
#include "components/download/public/common/download_item.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/widget/widget.h"

void DragDownloadItem(const download::DownloadItem* download,
                      const gfx::Image* icon,
                      gfx::NativeView native_view) {
  DCHECK_EQ(download::DownloadItem::COMPLETE, download->GetState());
  DCHECK(native_view);

  NSView* view = native_view.GetNativeNSView();
  NSPoint mouse_location = view.window.mouseLocationOutsideOfEventStream;

  views::Widget* widget = views::Widget::GetWidgetForNativeView(native_view);

  // If this drag was initiated from a views::Widget, that widget may have
  // mouse capture. Drags via View::DoDrag() usually release it. The code below
  // bypasses that, so release manually. See https://crbug.com/863377.
  if (widget) {
    widget->ReleaseCapture();
  }

  views::NativeWidgetMacNSWindowHost* host =
      views::NativeWidgetMacNSWindowHost::GetFromNativeView(native_view);
  if (!host) {
    DLOG(WARNING) << "DragDownloadItem: host is null";
    return;
  }

  remote_cocoa::mojom::NativeWidgetNSWindow* mojo_window =
      host->GetNSWindowMojo();
  if (!mojo_window) {
    DLOG(WARNING) << "DragDownloadItem: mojo_window is null";
    return;
  }

  auto file_drag_data = remote_cocoa::mojom::FileDragData::New();
  file_drag_data->file_path = download->GetTargetFilePath();

  if (icon && !icon->IsEmpty()) {
    file_drag_data->drag_image = icon->AsImageSkia();
    gfx::Size size = icon->Size();
    file_drag_data->image_offset =
        gfx::Vector2d(size.width() / 2, size.height() / 2);
  } else {
    file_drag_data->image_offset = gfx::Vector2d(8, 8);
  }

  gfx::PointF mouse_point(mouse_location.x, mouse_location.y);
  mojo_window->BeginFileDrag(std::move(file_drag_data), mouse_point);
}
