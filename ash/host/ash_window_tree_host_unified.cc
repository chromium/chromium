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
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/types/cxx23_to_underlying.h"
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

  // aura::WindowTargeter:
  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override {
    delegate_->SetCurrentEventTargeterSourceHost(nullptr);
    if (root == src_root_ && !event->target()) {
      return root;
    } else {
      NOTREACHED() << "event type:" << base::to_underlying(event->type());
    }
  }
  ui::EventSink* GetNewEventSinkForEvent(const ui::EventTarget* current_root,
                                         ui::EventTarget* target,
                                         ui::Event* in_out_event) override {
    if (current_root == src_root_ && !in_out_event->target()) {
      delegate_->SetCurrentEventTargeterSourceHost(src_root_->GetHost());
      if (in_out_event->IsLocatedEvent()) {
        ui::LocatedEvent* located_event = in_out_event->AsLocatedEvent();
        located_event->ConvertLocationToTarget(
            static_cast<aura::Window*>(nullptr), dst_root_.get());
      }
      return dst_root_->GetHost()->GetEventSink();
    }
    return nullptr;
  }

 private:
  raw_ptr<aura::Window> src_root_;
  raw_ptr<aura::Window> dst_root_;
  raw_ptr<AshWindowTreeHostDelegate> delegate_;  // Not owned.
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
  // TODO(b/356098565): Remove the log once the issue is fixed.
  LOG(ERROR) << "Creating Unified Desktop bounds=" << initial_bounds.ToString();
}

AshWindowTreeHostUnified::~AshWindowTreeHostUnified() {
  for (ash::AshWindowTreeHost* ash_host : mirroring_hosts_) {
    ash_host->AsWindowTreeHost()->window()->RemoveObserver(this);
  }
}

void AshWindowTreeHostUnified::PrepareForShutdown() {
  AshWindowTreeHostPlatform::PrepareForShutdown();

  for (ash::AshWindowTreeHost* host : mirroring_hosts_) {
    host->PrepareForShutdown();
  }
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
  for (ash::AshWindowTreeHost* host : mirroring_hosts_) {
    host->ClearCursorConfig();
  }
}

void AshWindowTreeHostUnified::SetCursorNative(gfx::NativeCursor cursor) {
  for (ash::AshWindowTreeHost* host : mirroring_hosts_) {
    host->AsWindowTreeHost()->SetCursor(cursor);
  }
}

void AshWindowTreeHostUnified::OnCursorVisibilityChangedNative(bool show) {
  for (ash::AshWindowTreeHost* host : mirroring_hosts_) {
    host->AsWindowTreeHost()->OnCursorVisibilityChanged(show);
  }
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
