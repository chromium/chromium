// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/ash_window_tree_host_unified.h"

#include <memory>
#include <tuple>
#include <utility>

#include "ash/host/ash_window_tree_host_delegate.h"
#include "ash/host/root_window_transformer.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/platform_window/stub/stub_window.h"

namespace ash {

class UnifiedEventTargeter : public aura::WindowTargeter {
 public:
  UnifiedEventTargeter(aura::Window* src_root,
                       aura::Window* dst_root,
                       AshWindowTreeHostDelegate* delegate)
      : src_root_(src_root), dst_root_(dst_root), delegate_(delegate) {
    DCHECK(delegate);
  }

  UnifiedEventTargeter(const UnifiedEventTargeter&) = delete;
  UnifiedEventTargeter& operator=(const UnifiedEventTargeter&) = delete;
  ~UnifiedEventTargeter() override { delegate_ = nullptr; }

  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override {
    if (root == src_root_ && !event->target()) {
      delegate_->SetCurrentEventTargeterSourceHost(src_root_->GetHost());

      if (event->IsLocatedEvent()) {
        ui::LocatedEvent* located_event = static_cast<ui::LocatedEvent*>(event);
        located_event->ConvertLocationToTarget(
            static_cast<aura::Window*>(nullptr), dst_root_);
      }
      auto ptr = weak_ptr_factory_.GetWeakPtr();
      std::ignore =
          dst_root_->GetHost()->GetEventSink()->OnEventFromSource(event);
      if (!ptr)
        return nullptr;

      // Reset the source host.
      delegate_->SetCurrentEventTargeterSourceHost(nullptr);

      return nullptr;
    } else {
      NOTREACHED() << "event type:" << event->type();
      return aura::WindowTargeter::FindTargetForEvent(root, event);
    }
  }

 private:
  aura::Window* src_root_;
  aura::Window* dst_root_;
  AshWindowTreeHostDelegate* delegate_;  // Not owned.
  base::WeakPtrFactory<UnifiedEventTargeter> weak_ptr_factory_{this};
};

AshWindowTreeHostUnified::AshWindowTreeHostUnified(
    const gfx::Rect& initial_bounds,
    AshWindowTreeHostDelegate* delegate,
    size_t compositor_memory_limit_mb)
    : AshWindowTreeHostPlatform(
          std::make_unique<ui::StubWindow>(initial_bounds),
          delegate,
          compositor_memory_limit_mb) {
  ui::StubWindow* stub_window = static_cast<ui::StubWindow*>(platform_window());
  stub_window->InitDelegate(this, true);
}

AshWindowTreeHostUnified::~AshWindowTreeHostUnified() {
  for (auto* ash_host : mirroring_hosts_)
    ash_host->AsWindowTreeHost()->window()->RemoveObserver(this);
}

void AshWindowTreeHostUnified::PrepareForShutdown() {
  AshWindowTreeHostPlatform::PrepareForShutdown();

  for (auto* host : mirroring_hosts_)
    host->PrepareForShutdown();
}

void AshWindowTreeHostUnified::RegisterMirroringHost(
    AshWindowTreeHost* mirroring_ash_host) {
  aura::Window* src_root = mirroring_ash_host->AsWindowTreeHost()->window();
  src_root->SetEventTargeter(
      std::make_unique<UnifiedEventTargeter>(src_root, window(), delegate_));
  DCHECK(!base::Contains(mirroring_hosts_, mirroring_ash_host));
  mirroring_hosts_.push_back(mirroring_ash_host);
  src_root->AddObserver(this);
  mirroring_ash_host->UpdateCursorConfig();
}

// Do nothing, since mirroring hosts had their cursor config updated when they
// were registered.
void AshWindowTreeHostUnified::UpdateCursorConfig() {}

void AshWindowTreeHostUnified::ClearCursorConfig() {
  for (auto* host : mirroring_hosts_)
    host->ClearCursorConfig();
}

void AshWindowTreeHostUnified::SetCursorNative(gfx::NativeCursor cursor) {
  for (auto* host : mirroring_hosts_)
    host->AsWindowTreeHost()->SetCursor(cursor);
}

void AshWindowTreeHostUnified::OnCursorVisibilityChangedNative(bool show) {
  for (auto* host : mirroring_hosts_)
    host->AsWindowTreeHost()->OnCursorVisibilityChanged(show);
}

void AshWindowTreeHostUnified::OnBoundsChanged(const BoundsChange& change) {
  if (platform_window())
    OnHostResizedInPixels(platform_window()->GetBoundsInPixels().size());
}

void AshWindowTreeHostUnified::OnWindowDestroying(aura::Window* window) {
  auto iter = base::ranges::find(
      mirroring_hosts_, window, [](AshWindowTreeHost* ash_host) {
        return ash_host->AsWindowTreeHost()->window();
      });
  DCHECK(iter != mirroring_hosts_.end());
  window->RemoveObserver(this);
  mirroring_hosts_.erase(iter);
}

}  // namespace ash
