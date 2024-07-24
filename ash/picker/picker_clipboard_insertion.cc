// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_clipboard_insertion.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/clipboard/clipboard_history_util.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/public/cpp/scoped_clipboard_history_pause.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#include "ui/events/types/event_type.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/events/keycodes/xkb_keysym.h"
#endif

namespace ash {

namespace {

// Fork of ash/clipboard/clipboard_history_controller_impl.cc.

#if BUILDFLAG(USE_XKBCOMMON)
// Looks up the DomCode assigned to the keysym. In some edge cases,
// such as Dvorak layout, the original DomCode may be different
// from US standard layout.
ui::DomCode LookUpXkbDomCode(int keysym) {
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    // On linux-chromeos, stub layout engine is used.
    return ui::DomCode::NONE;
  }
  ui::KeyboardLayoutEngine* layout_engine =
      ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine();
  if (!layout_engine) {
    return ui::DomCode::NONE;
  }
  return static_cast<ui::XkbKeyboardLayoutEngine*>(layout_engine)
      ->GetDomCodeByKeysym(keysym, /*modifiers=*/std::nullopt);
}
#endif

ui::KeyEvent SyntheticCtrlV(ui::EventType type) {
  ui::DomCode dom_code = ui::DomCode::NONE;
#if BUILDFLAG(USE_XKBCOMMON)
  dom_code = LookUpXkbDomCode(XKB_KEY_v);
#endif
  return dom_code == ui::DomCode::NONE
             ? ui::KeyEvent(type, ui::VKEY_V, ui::EF_CONTROL_DOWN)
             : ui::KeyEvent(type, ui::VKEY_V, dom_code, ui::EF_CONTROL_DOWN);
}

ui::KeyEvent SyntheticCtrl(ui::EventType type) {
  int flags =
      type == ui::EventType::kKeyPressed ? ui::EF_CONTROL_DOWN : ui::EF_NONE;
  ui::DomCode dom_code = ui::DomCode::NONE;
#if BUILDFLAG(USE_XKBCOMMON)
  dom_code = LookUpXkbDomCode(XKB_KEY_Control_L);
#endif
  return dom_code == ui::DomCode::NONE
             ? ui::KeyEvent(type, ui::VKEY_CONTROL, flags)
             : ui::KeyEvent(type, ui::VKEY_CONTROL, dom_code, flags);
}

std::unique_ptr<ui::ClipboardData> ReplaceClipboard(
    std::unique_ptr<ui::ClipboardData> data) {
  // Pause changes to clipboard history while manipulating the clipboard.
  std::unique_ptr<ash::ScopedClipboardHistoryPause> pause_history =
      CHECK_DEREF(ash::ClipboardHistoryController::Get()).CreateScopedPause();
  return CHECK_DEREF(ui::ClipboardNonBacked::GetForCurrentThread())
      .WriteClipboardData(std::move(data));
}

// `intended_window` must be non-null.
// Forked from `PasteClipboardHistoryItem`.
void InsertClipboardDataImpl(aura::Window* intended_window,
                             std::unique_ptr<ui::ClipboardData> data,
                             base::OnceCallback<void(bool)> done_callback) {
  if (intended_window != window_util::GetActiveWindow()) {
    std::move(done_callback).Run(false);
    return;
  }

  std::unique_ptr<ui::ClipboardData> replaced_data =
      ReplaceClipboard(std::move(data));

  // TODO: b/351050139 - Use a direct paste into web contents if possible,
  // instead of always using "synthetic paste".
  aura::WindowTreeHost* host = intended_window->GetHost();
  CHECK(host);

  ui::KeyEvent ctrl_press = SyntheticCtrl(ui::EventType::kKeyPressed);
  host->DeliverEventToSink(&ctrl_press);

  ui::KeyEvent v_press = SyntheticCtrlV(ui::EventType::kKeyPressed);
  host->DeliverEventToSink(&v_press);

  ui::KeyEvent v_release = SyntheticCtrlV(ui::EventType::kKeyReleased);
  host->DeliverEventToSink(&v_release);

  ui::KeyEvent ctrl_release = SyntheticCtrl(ui::EventType::kKeyReleased);
  host->DeliverEventToSink(&ctrl_release);

  std::move(done_callback).Run(true);

  if (!replaced_data) {
    // No was on the clipboard.
    return;
  }

  // Restore the clipboard data asynchronously. Some apps take a long time to
  // receive the paste event, and some apps will read from the clipboard
  // multiple times per paste. Wait a bit before writing `replaced_data` back to
  // the clipboard.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<ui::ClipboardData> data) {
            ReplaceClipboard(std::move(data));
          },
          std::move(replaced_data)),
      base::Milliseconds(200));
}

}  // namespace

// Forked from `ClipboardHistoryControllerImpl::MaybePastePostTask`.
void InsertClipboardData(std::unique_ptr<ui::ClipboardData> data,
                         base::OnceCallback<void(bool)> done_callback) {
  if (aura::Window* active_window = window_util::GetActiveWindow()) {
    // Paste asynchronously to ensure ARC windows handle paste events correctly.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&InsertClipboardDataImpl, active_window,
                                  std::move(data), std::move(done_callback)));
  } else {
    std::move(done_callback).Run(false);
  }
}

}  // namespace ash
