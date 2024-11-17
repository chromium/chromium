// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/screen_manager_ash.h"

#include <map>
#include <utility>
#include <vector>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "base/containers/adapters.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "components/exo/shell_surface_util.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/desktop_media_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/video_capture/ash/video_capture_device_ash.h"
#include "ui/aura/window_observer.h"
#include "ui/snapshot/snapshot.h"

namespace crosapi {
namespace {

// This class tracks the set of known windows and associates an ID with each.
class WindowList : public aura::WindowObserver {
 public:
  WindowList() = default;
  ~WindowList() override {
    for (auto pair : window_to_id_)
      pair.first->RemoveObserver(this);
  }

  uint64_t LookupOrAddId(aura::Window* window) {
    auto it = window_to_id_.find(window);
    if (it != window_to_id_.end())
      return it->second;
    id_to_window_[++next_window_id_] = window;
    window_to_id_[window] = next_window_id_;
    window->AddObserver(this);
    return next_window_id_;
  }

  aura::Window* LookupWindow(uint64_t id) {
    auto it = id_to_window_.find(id);
    if (it == id_to_window_.end())
      return nullptr;
    return it->second;
  }

  // aura::WindowObserver
  // This method is overridden purely to remove dead windows from
  // |id_to_window_| and |window_to_id_|. This ensures that if the pointer is
  // reused for a new window, it does not get confused with a previous window.
  void OnWindowDestroying(aura::Window* window) override {
    auto it = window_to_id_.find(window);
    if (it == window_to_id_.end())
      return;
    uint64_t id = it->second;
    window_to_id_.erase(it);
    id_to_window_.erase(id);
  }

 private:
  // This class generates unique, non-reused IDs for windows on demand. The IDs
  // are monotonically increasing 64-bit integers. Once an ID is assigned to a
  // window, this class listens for the destruction of the window in order to
  // remove dead windows from the map.
  //
  // The members |id_to_window_| and |window_to_id_| must be kept in sync. The
  // members exist to allow fast lookup in both directions.
  std::map<uint64_t, raw_ptr<aura::Window, CtnExperimental>> id_to_window_;
  std::map<aura::Window*, uint64_t> window_to_id_;
  uint64_t next_window_id_ = 0;
};

class SnapshotCapturerBase : public mojom::SnapshotCapturer {
 public:
  SnapshotCapturerBase() = default;
  ~SnapshotCapturerBase() override = default;

  void BindReceiver(
      mojo::PendingReceiver<mojom::SnapshotCapturer> pending_receiver) {
    receivers_.Add(this, std::move(pending_receiver));
  }

  void TakeSnapshot(uint64_t id, TakeSnapshotCallback callback) override {
    aura::Window* window = LookupWindow(id);
    if (!window) {
      std::move(callback).Run(/*success=*/false, SkBitmap());
      return;
    }

    gfx::Rect bounds = window->bounds();
    bounds.set_x(0);
    bounds.set_y(0);
    ui::GrabWindowSnapshot(
        window, bounds,
        base::BindOnce(
            [](TakeSnapshotCallback callback, gfx::Image image) {
              std::move(callback).Run(/*success=*/true, image.AsBitmap());
            },
            std::move(callback)));
  }

  aura::Window* LookupWindow(uint64_t id) {
    return window_list_.LookupWindow(id);
  }

 protected:
  WindowList window_list_;

 private:
  mojo::ReceiverSet<mojom::SnapshotCapturer> receivers_;
};

}  // namespace

class ScreenManagerAsh::ScreenCapturerImpl : public SnapshotCapturerBase {
 public:
  ScreenCapturerImpl() = default;
  ~ScreenCapturerImpl() override = default;

  void ListSources(ListSourcesCallback callback) override {
    std::vector<mojom::SnapshotSourcePtr> sources;

    aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();
    for (aura::Window* root_window : root_windows) {
      mojom::SnapshotSourcePtr source = mojom::SnapshotSource::New();
      source->id = window_list_.LookupOrAddId(root_window);
      source->title = base::UTF16ToUTF8(root_window->GetTitle());
      source->display_id = display::Screen::GetScreen()
                               ->GetDisplayNearestWindow(root_window)
                               .id();

      if (root_window == ash::Shell::GetPrimaryRootWindow()) {
        sources.insert(sources.begin(), std::move(source));
      } else {
        sources.push_back(std::move(source));
      }
    }

    std::move(callback).Run(std::move(sources));
  }

  uint64_t GetPrimaryRootWindowId() {
    return window_list_.LookupOrAddId(ash::Shell::GetPrimaryRootWindow());
  }
};

class ScreenManagerAsh::WindowCapturerImpl : public SnapshotCapturerBase {
 public:
  WindowCapturerImpl() = default;
  ~WindowCapturerImpl() override = default;

  void ListSources(ListSourcesCallback callback) override {
    // We need to create a vector that contains window_id and title.
    std::vector<mojom::SnapshotSourcePtr> sources;

    aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();
    for (aura::Window* root_window : root_windows) {
      // The list of desks containers depends on whether the Virtual Desks
      // feature is enabled or not.
      for (int desk_id : ash::desks_util::GetDesksContainersIds())
        AppendWindowsForRoot(root_window, desk_id, &sources);

      AppendWindowsForRoot(root_window,
                           ash::kShellWindowId_AlwaysOnTopContainer, &sources);
    }

    std::move(callback).Run(std::move(sources));
  }

 private:
  void AppendWindowsForRoot(aura::Window* root_window,
                            int container_id,
                            std::vector<mojom::SnapshotSourcePtr>* sources) {
    aura::Window* container =
        ash::Shell::GetContainer(root_window, container_id);
    if (!container)
      return;

    // The |container| has all the top-level windows in reverse order, e.g.
    // the most top-level window is at the end. So iterate children reversely
    // to make sure |windows| is in the expected order.
    for (aura::Window* window : base::Reversed(container->children())) {
      // TODO(crbug.com/40135428): The window is currently not visible
      // or focusable. If the window later becomes invisible or unfocusable,
      // we don't bother removing the window from the list. We should handle
      // this more robustly.
      if (!window->IsVisible() || !window->CanFocus())
        continue;

      mojom::SnapshotSourcePtr source = mojom::SnapshotSource::New();
      source->id = window_list_.LookupOrAddId(window);
      source->title = base::UTF16ToUTF8(window->GetTitle());
      if (browser_util::IsLacrosWindow(window)) {
        const std::string* app_id = exo::GetShellApplicationId(window);
        DCHECK(app_id);
        source->window_unique_id = *app_id;
      }

      sources->push_back(std::move(source));
    }
  }
};

ScreenManagerAsh::ScreenManagerAsh() = default;
ScreenManagerAsh::~ScreenManagerAsh() = default;

void ScreenManagerAsh::BindReceiver(
    mojo::PendingReceiver<mojom::ScreenManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ScreenManagerAsh::DeprecatedTakeScreenSnapshot(
    DeprecatedTakeScreenSnapshotCallback callback) {
  NOTIMPLEMENTED();
}
void ScreenManagerAsh::DeprecatedListWindows(
    DeprecatedListWindowsCallback callback) {
  NOTIMPLEMENTED();
}
void ScreenManagerAsh::DeprecatedTakeWindowSnapshot(
    uint64_t id,
    DeprecatedTakeWindowSnapshotCallback callback) {
  NOTIMPLEMENTED();
}
void ScreenManagerAsh::GetScreenCapturer(
    mojo::PendingReceiver<mojom::SnapshotCapturer> receiver) {
  GetScreenCapturerImpl()->BindReceiver(std::move(receiver));
}

void ScreenManagerAsh::GetWindowCapturer(
    mojo::PendingReceiver<mojom::SnapshotCapturer> receiver) {
  GetWindowCapturerImpl()->BindReceiver(std::move(receiver));
}

void ScreenManagerAsh::GetScreenVideoCapturer(
    mojo::PendingReceiver<mojom::VideoCaptureDevice> receiver,
    uint64_t screen_id) {
  // Only one instance of ScreenManagerAsh exists; so a window capturer must
  // have been created in order to list the available sources. Otherwise, the
  // passed-in id is invalid, as we have no way of translating it to a window.
  if (!screen_capturer_impl_)
    return;
  aura::Window* window = screen_capturer_impl_->LookupWindow(screen_id);
  if (!window)
    return;

  content::DesktopMediaID id = content::DesktopMediaID::RegisterNativeWindow(
      content::DesktopMediaID::TYPE_SCREEN, window);

  CreateVideoCaptureDevice(std::move(receiver), id);
}

void ScreenManagerAsh::GetWindowVideoCapturer(
    mojo::PendingReceiver<mojom::VideoCaptureDevice> receiver,
    uint64_t window_id) {
  // Only one instance of ScreenManagerAsh exists; so a window capturer must
  // have been created in order to list the available sources. Otherwise, the
  // passed-in id is invalid, as we have no way of translating it to a window.
  if (!window_capturer_impl_)
    return;

  aura::Window* window = window_capturer_impl_->LookupWindow(window_id);
  if (!window)
    return;

  content::DesktopMediaID id = content::DesktopMediaID::RegisterNativeWindow(
      content::DesktopMediaID::TYPE_WINDOW, window);

  CreateVideoCaptureDevice(std::move(receiver), id);
}

aura::Window* ScreenManagerAsh::GetWindowById(uint64_t id) const {
  if (!window_capturer_impl_)
    return nullptr;

  return window_capturer_impl_->LookupWindow(id);
}

ScreenManagerAsh::ScreenCapturerImpl*
ScreenManagerAsh::GetScreenCapturerImpl() {
  if (!screen_capturer_impl_)
    screen_capturer_impl_ = std::make_unique<ScreenCapturerImpl>();
  return screen_capturer_impl_.get();
}

ScreenManagerAsh::WindowCapturerImpl*
ScreenManagerAsh::GetWindowCapturerImpl() {
  if (!window_capturer_impl_)
    window_capturer_impl_ = std::make_unique<WindowCapturerImpl>();
  return window_capturer_impl_.get();
}

void ScreenManagerAsh::CreateVideoCaptureDevice(
    mojo::PendingReceiver<mojom::VideoCaptureDevice> receiver,
    const content::DesktopMediaID& device_id) {
  // We can have multiple captures ongoing at the same time; we make this as a
  // self-owned receiver so that the Lacros side code can control its lifetime
  // by shutting down its remote.
  mojo::PendingReceiver<video_capture::mojom::Device> device_receiver;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<VideoCaptureDeviceAsh>(
          device_receiver.InitWithNewPipeAndPassRemote()),
      std::move(receiver));
  content::desktop_capture::BindAuraWindowCapturer(std::move(device_receiver),
                                                   device_id);
}

}  // namespace crosapi
