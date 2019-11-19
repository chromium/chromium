// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/desktop_media_list_ash.h"

#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "base/bind.h"
#include "chrome/grit/generated_resources.h"
#include "media/base/video_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"

using content::DesktopMediaID;

namespace {

// Update the list twice per second.
const int kDefaultDesktopMediaListUpdatePeriod = 500;

}  // namespace

DesktopMediaListAsh::DesktopMediaListAsh(content::DesktopMediaID::Type type)
    : DesktopMediaListBase(base::TimeDelta::FromMilliseconds(
          kDefaultDesktopMediaListUpdatePeriod)) {
  DCHECK(type == content::DesktopMediaID::TYPE_SCREEN ||
         type == content::DesktopMediaID::TYPE_WINDOW);
  type_ = type;
}

DesktopMediaListAsh::~DesktopMediaListAsh() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DesktopMediaListAsh::Refresh(bool update_thumnails) {
  DCHECK(can_refresh());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(pending_window_capture_requests_, 0);

  std::vector<SourceDescription> new_sources;
  EnumerateSources(&new_sources, update_thumnails);
  UpdateSourcesList(new_sources);
  OnRefreshMaybeComplete();
}

void DesktopMediaListAsh::EnumerateWindowsForRoot(
    std::vector<DesktopMediaListAsh::SourceDescription>* sources,
    bool update_thumnails,
    aura::Window* root_window,
    int container_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  aura::Window* container = ash::Shell::GetContainer(root_window, container_id);
  if (!container)
    return;
  // The |container| has all the top-level windows in reverse order, e.g. the
  // most top-level window is at the end. So iterate children reversely to make
  // sure |sources| is in the expected order.
  for (aura::Window::Windows::const_reverse_iterator it =
           container->children().rbegin();
       it != container->children().rend(); ++it) {
    if (!(*it)->IsVisible() || !(*it)->CanFocus())
      continue;
    content::DesktopMediaID id = content::DesktopMediaID::RegisterNativeWindow(
        content::DesktopMediaID::TYPE_WINDOW, *it);
    if (id.window_id == view_dialog_id_.window_id)
      continue;
    SourceDescription window_source(id, (*it)->GetTitle());
    sources->push_back(window_source);

    if (update_thumnails)
      CaptureThumbnail(window_source.id, *it);
  }
}

void DesktopMediaListAsh::EnumerateSources(
    std::vector<DesktopMediaListAsh::SourceDescription>* sources,
    bool update_thumnails) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();

  for (size_t i = 0; i < root_windows.size(); ++i) {
    if (type_ == content::DesktopMediaID::TYPE_SCREEN) {
      SourceDescription screen_source(
          content::DesktopMediaID::RegisterNativeWindow(
              content::DesktopMediaID::TYPE_SCREEN, root_windows[i]),
          root_windows[i]->GetTitle());

      if (root_windows[i] == ash::Shell::GetPrimaryRootWindow())
        sources->insert(sources->begin(), screen_source);
      else
        sources->push_back(screen_source);

      if (screen_source.name.empty()) {
        if (root_windows.size() > 1) {
          // 'Screen' in 'Screen 1, Screen 2, etc ' might be inflected in some
          // languages depending on the number although rather unlikely. To be
          // safe, use the plural format.
          // TODO(jshin): Revert to GetStringFUTF16Int (with native digits)
          // if none of UI languages inflects 'Screen' in this context.
          screen_source.name = l10n_util::GetPluralStringFUTF16(
              IDS_DESKTOP_MEDIA_PICKER_MULTIPLE_SCREEN_NAME,
              static_cast<int>(i + 1));
        } else {
          screen_source.name = l10n_util::GetStringUTF16(
              IDS_DESKTOP_MEDIA_PICKER_SINGLE_SCREEN_NAME);
        }
      }

      if (update_thumnails)
        CaptureThumbnail(screen_source.id, root_windows[i]);
    } else {
      // The list of desks containers depends on whether the Virtual Desks
      // feature is enabled or not.
      for (int desk_id : ash::desks_util::GetDesksContainersIds())
        EnumerateWindowsForRoot(sources, update_thumnails, root_windows[i],
                                desk_id);

      EnumerateWindowsForRoot(sources, update_thumnails, root_windows[i],
                              ash::kShellWindowId_AlwaysOnTopContainer);
      EnumerateWindowsForRoot(sources, update_thumnails, root_windows[i],
                              ash::kShellWindowId_PipContainer);
    }
  }
}

void DesktopMediaListAsh::CaptureThumbnail(content::DesktopMediaID id,
                                           aura::Window* window) {
  gfx::Rect window_rect(window->bounds().width(), window->bounds().height());
  gfx::Rect scaled_rect = media::ComputeLetterboxRegion(
      gfx::Rect(thumbnail_size_), window_rect.size());

  ++pending_window_capture_requests_;
  ui::GrabWindowSnapshotAndScaleAsync(
      window, window_rect, scaled_rect.size(),
      base::Bind(&DesktopMediaListAsh::OnThumbnailCaptured,
                 weak_factory_.GetWeakPtr(), id));
}

void DesktopMediaListAsh::OnThumbnailCaptured(content::DesktopMediaID id,
                                              gfx::Image image) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UpdateSourceThumbnail(id, image.AsImageSkia());

  --pending_window_capture_requests_;
  DCHECK_GE(pending_window_capture_requests_, 0);

  OnRefreshMaybeComplete();
}

void DesktopMediaListAsh::OnRefreshMaybeComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (pending_window_capture_requests_ == 0) {
    // Once we've finished capturing all windows, notify the caller, which will
    // post a task for the next list update if necessary.
    OnRefreshComplete();
  }
}
