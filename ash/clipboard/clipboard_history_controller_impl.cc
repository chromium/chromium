// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_controller_impl.h"

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/clipboard/clipboard_history_controller_delegate.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_history_menu_model_adapter.h"
#include "ash/clipboard/clipboard_history_resource_manager.h"
#include "ash/clipboard/clipboard_history_url_title_fetcher.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/clipboard_nudge_constants.h"
#include "ash/clipboard/clipboard_nudge_controller.h"
#include "ash/clipboard/scoped_clipboard_history_pause_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/display/display_util.h"
#include "ash/public/cpp/clipboard_image_model_factory.h"
#include "ash/public/cpp/window_tree_host_lookup.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/color_util.h"
#include "ash/wm/window_util.h"
#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/one_shot_event.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/clipboard/clipboard_util.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/color/color_provider_source.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/menu/menu_controller.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/events/keycodes/xkb_keysym.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#endif

namespace ash {

namespace {

// Encodes `bitmap` and maps the corresponding ClipboardHistoryItem ID, `id, to
// the resulting PNG in `encoded_pngs`. This function should run on a background
// thread.
void EncodeBitmapToPNG(
    base::OnceClosure barrier_callback,
    std::map<base::UnguessableToken, std::vector<uint8_t>>* const encoded_pngs,
    base::UnguessableToken id,
    SkBitmap bitmap) {
  auto png = ui::clipboard_util::EncodeBitmapToPng(bitmap);

  // Don't acquire the lock until after the image encoding has finished.
  static base::NoDestructor<base::Lock> map_lock;
  base::AutoLock lock(*map_lock);

  encoded_pngs->emplace(id, std::move(png));
  std::move(barrier_callback).Run();
}

// Returns the clipboard instance for the current thread.
ui::ClipboardNonBacked* GetClipboard() {
  auto* clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
  DCHECK(clipboard);
  return clipboard;
}

// Returns the last active user pref service or `nullptr` if one does not exist.
PrefService* GetLastActiveUserPrefService() {
  return Shell::Get()->session_controller()->GetLastActiveUserPrefService();
}

// Returns the time when the menu was last shown for the user associated with
// the last active user pref service, or `std::nullopt` if the menu was not
// previously marked as having been shown.
std::optional<base::Time> GetMenuLastTimeShown() {
  if (auto* prefs = GetLastActiveUserPrefService()) {
    if (auto* pref = prefs->FindPreference(prefs::kMultipasteMenuLastTimeShown);
        pref && !pref->IsDefaultValue()) {
      return base::ValueToTime(pref->GetValue());
    }
  }
  return std::nullopt;
}

// Marks the time when the menu was last shown for the user associated with the
// last active user pref service.
void MarkMenuLastTimeShown() {
  if (auto* prefs = GetLastActiveUserPrefService()) {
    prefs->SetTime(prefs::kMultipasteMenuLastTimeShown, base::Time::Now());
  }
}

// Emits a user action indicating that the clipboard history item at menu index
// `command_id` was pasted.
void RecordMenuIndexPastedUserAction(int command_id) {
  // Per guidance in user_metrics.h, use string literals for action names.
  switch (command_id) {
    case 1:
      base::RecordAction(
          base::UserMetricsAction("Ash_ClipboardHistory_PastedItem1"));
      break;
    case 2:
      base::RecordAction(
          base::UserMetricsAction("Ash_ClipboardHistory_PastedItem2"));
      break;
    case 3:
      base::RecordAction(
          base::UserMetricsAction("Ash_ClipboardHistory_PastedItem3"));
      break;
    case 4:
      base::RecordAction(
          base::UserMetricsAction("Ash_ClipboardHistory_PastedItem4"));
      break;
    case 5:
      base::RecordAction(
          base::UserMetricsAction("Ash_ClipboardHistory_PastedItem5"));
      break;
    default:
      NOTREACHED();
  }
}

void RecordPasteItemIndex(int index) {
  CHECK_GE(index, clipboard_history_util::kFirstItemCommandId);
  CHECK_LT(index, clipboard_history_util::kCommandIdBoundary);

  // Record the paste item's history list index in a histogram to get a
  // distribution of where in the list users paste from.
  base::UmaHistogramExactLinear(
      "Ash.ClipboardHistory.ContextMenu.MenuOptionSelected", index,
      /*exclusive_max=*/clipboard_history_util::kCommandIdBoundary);

  // Record the paste item's history list index as a user action to analyze
  // usage patterns, e.g., how frequently the same index is pasted multiple
  // times in a row.
  RecordMenuIndexPastedUserAction(index);
}

#if BUILDFLAG(USE_XKBCOMMON)
// Looks up the DomCode assigned to the keysym. In some edge cases,
// such as Dvorak layout, the original DomCode may be different
// from US standard layout.
ui::DomCode LookUpXkbDomCode(int keysym) {
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    // On linux-chromeos, stub layout engine is used.
    return ui::DomCode::NONE;
  }
  auto* layout_engine =
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

void SyntheticPaste(
    crosapi::mojom::ClipboardHistoryControllerShowSource paste_source) {
  auto* host = GetWindowTreeHostForDisplay(
      display::Screen::GetScreen()->GetDisplayForNewWindows().id());
  CHECK(host);

  // Because we do not require the user to release Ctrl+V before selecting a
  // clipboard history item to paste, the Ctrl+V event we synthesize below may
  // be discarded as a perceived continuation of the long press. Preempt this
  // scenario by issuing a Ctrl+V release to ensure that the press and release
  // below are handled as an independent paste.
  // TODO(http://b/283533126): Replace this workaround with a long-term fix.
  if (paste_source == crosapi::mojom::ClipboardHistoryControllerShowSource::
                          kControlVLongpress) {
    ui::KeyEvent v_release = SyntheticCtrlV(ui::EventType::kKeyReleased);
    host->DeliverEventToSink(&v_release);

    ui::KeyEvent ctrl_release = SyntheticCtrl(ui::EventType::kKeyReleased);
    host->DeliverEventToSink(&ctrl_release);
  }

  ui::KeyEvent ctrl_press = SyntheticCtrl(ui::EventType::kKeyPressed);
  host->DeliverEventToSink(&ctrl_press);

  ui::KeyEvent v_press = SyntheticCtrlV(ui::EventType::kKeyPressed);
  host->DeliverEventToSink(&v_press);

  ui::KeyEvent v_release = SyntheticCtrlV(ui::EventType::kKeyReleased);
  host->DeliverEventToSink(&v_release);

  ui::KeyEvent ctrl_release = SyntheticCtrl(ui::EventType::kKeyReleased);
  host->DeliverEventToSink(&ctrl_release);
}

using ClipboardHistoryPasteType =
    ClipboardHistoryControllerImpl::ClipboardHistoryPasteType;
bool IsPlainTextPaste(ClipboardHistoryPasteType paste_type) {
  switch (paste_type) {
    case ClipboardHistoryPasteType::kPlainTextAccelerator:
    case ClipboardHistoryPasteType::kPlainTextKeystroke:
    case ClipboardHistoryPasteType::kPlainTextMouse:
    case ClipboardHistoryPasteType::kPlainTextTouch:
    case ClipboardHistoryPasteType::kPlainTextVirtualKeyboard:
    case ClipboardHistoryPasteType::kPlainTextCtrlV:
      return true;
    case ClipboardHistoryPasteType::kRichTextAccelerator:
    case ClipboardHistoryPasteType::kRichTextKeystroke:
    case ClipboardHistoryPasteType::kRichTextMouse:
    case ClipboardHistoryPasteType::kRichTextTouch:
    case ClipboardHistoryPasteType::kRichTextVirtualKeyboard:
    case ClipboardHistoryPasteType::kRichTextCtrlV:
      return false;
  }
}

ClipboardHistoryPasteType CalculatePasteType(
    crosapi::mojom::ClipboardHistoryControllerShowSource paste_source,
    int event_flags) {
  // There are no specific flags that indicate a paste triggered by a
  // keystroke, so assume by default that keystroke was the event source
  // and then check for the other known possibilities. This assumption may
  // cause pastes from unknown sources to be incorrectly captured as
  // keystroke pastes, but we do not expect such cases to significantly
  // alter metrics.
  const bool paste_plain_text = event_flags & ui::EF_SHIFT_DOWN;

  if (paste_source ==
      crosapi::mojom::ClipboardHistoryControllerShowSource::kVirtualKeyboard) {
    return paste_plain_text
               ? ClipboardHistoryPasteType::kPlainTextVirtualKeyboard
               : ClipboardHistoryPasteType::kRichTextVirtualKeyboard;
  }

  ClipboardHistoryPasteType paste_type =
      paste_plain_text ? ClipboardHistoryPasteType::kPlainTextKeystroke
                       : ClipboardHistoryPasteType::kRichTextKeystroke;
  if (event_flags & ui::EF_MOUSE_BUTTON) {
    paste_type = paste_plain_text ? ClipboardHistoryPasteType::kPlainTextMouse
                                  : ClipboardHistoryPasteType::kRichTextMouse;
  } else if (event_flags & ui::EF_FROM_TOUCH) {
    paste_type = paste_plain_text ? ClipboardHistoryPasteType::kPlainTextTouch
                                  : ClipboardHistoryPasteType::kRichTextTouch;
  }
  return paste_type;
}

}  // namespace

// ClipboardHistoryControllerImpl::AcceleratorTarget ---------------------------

class ClipboardHistoryControllerImpl::AcceleratorTarget
    : public ui::AcceleratorTarget {
 public:
  explicit AcceleratorTarget(ClipboardHistoryControllerImpl* controller)
      : controller_(controller),
        delete_selected_(ui::Accelerator(
            /*key_code=*/ui::VKEY_BACK,
            /*modifiers=*/ui::EF_NONE,
            /*key_state=*/ui::Accelerator::KeyState::PRESSED)),
        tab_navigation_(ui::Accelerator(
            /*key_code=*/ui::VKEY_TAB,
            /*modifiers=*/ui::EF_NONE,
            /*key_state=*/ui::Accelerator::KeyState::PRESSED)),
        shift_tab_navigation_(ui::Accelerator(
            /*key_code=*/ui::VKEY_TAB,
            /*modifiers=*/ui::EF_SHIFT_DOWN,
            /*key_state=*/ui::Accelerator::KeyState::PRESSED)),
        paste_first_item_(ui::Accelerator(
            /*key_code=*/ui::VKEY_V,
            /*modifiers=*/ui::EF_CONTROL_DOWN,
            /*key_state=*/ui::Accelerator::KeyState::PRESSED)),
        paste_first_item_plaintext_(ui::Accelerator(
            /*key_code=*/ui::VKEY_V,
            /*modifiers=*/ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
            /*key_state=*/ui::Accelerator::KeyState::PRESSED)) {}
  AcceleratorTarget(const AcceleratorTarget&) = delete;
  AcceleratorTarget& operator=(const AcceleratorTarget&) = delete;
  ~AcceleratorTarget() override = default;

  void OnMenuShown() {
    Shell::Get()->accelerator_controller()->Register(
        {delete_selected_, tab_navigation_, shift_tab_navigation_,
         paste_first_item_, paste_first_item_plaintext_},
        /*target=*/this);
  }

  void OnMenuClosed() {
    Shell::Get()->accelerator_controller()->UnregisterAll(/*target=*/this);
  }

 private:
  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    CHECK(controller_->IsMenuShowing());

    if (accelerator == delete_selected_) {
      HandleDeleteSelected();
    } else if (accelerator == tab_navigation_) {
      HandleTab();
    } else if (accelerator == shift_tab_navigation_) {
      HandleShiftTab();
    } else if (accelerator == paste_first_item_) {
      HandlePasteFirstItem(ClipboardHistoryPasteType::kRichTextCtrlV);
    } else if (accelerator == paste_first_item_plaintext_) {
      HandlePasteFirstItem(ClipboardHistoryPasteType::kPlainTextCtrlV);
    } else {
      NOTREACHED();
    }

    return true;
  }

  bool CanHandleAccelerators() const override {
    return controller_->IsMenuShowing() ||
           controller_->HasAvailableHistoryItems();
  }

  void HandleDeleteSelected() { controller_->DeleteSelectedMenuItemIfAny(); }

  void HandleTab() { controller_->AdvancePseudoFocus(/*reverse=*/false); }

  void HandleShiftTab() { controller_->AdvancePseudoFocus(/*reverse=*/true); }

  void HandlePasteFirstItem(ClipboardHistoryPasteType paste_type) {
    const auto first_item_command_id =
        controller_->context_menu_->GetFirstMenuItemCommand();
    CHECK(first_item_command_id);
    controller_->PasteClipboardItemByCommandId(*first_item_command_id,
                                               paste_type);
  }

  // The controller responsible for showing the clipboard history menu.
  const raw_ptr<ClipboardHistoryControllerImpl> controller_;

  // Deletes the selected menu item.
  const ui::Accelerator delete_selected_;

  // Moves the pseudo focus forward.
  const ui::Accelerator tab_navigation_;

  // Moves the pseudo focus backward.
  const ui::Accelerator shift_tab_navigation_;

  // Pastes the first item in the clipboard history menu.
  const ui::Accelerator paste_first_item_;

  // Pastes the plain text data of the first item in the clipboard history menu.
  const ui::Accelerator paste_first_item_plaintext_;
};

// ClipboardHistoryControllerImpl::MenuDelegate --------------------------------

class ClipboardHistoryControllerImpl::MenuDelegate
    : public ui::SimpleMenuModel::Delegate {
 public:
  explicit MenuDelegate(ClipboardHistoryControllerImpl* controller)
      : controller_(controller) {}
  MenuDelegate(const MenuDelegate&) = delete;
  MenuDelegate& operator=(const MenuDelegate&) = delete;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    controller_->ExecuteCommand(command_id, event_flags);
  }

 private:
  // The controller responsible for showing the Clipboard History menu.
  const raw_ptr<ClipboardHistoryControllerImpl> controller_;
};

// ClipboardHistoryControllerImpl ----------------------------------------------

ClipboardHistoryControllerImpl::ClipboardHistoryControllerImpl(
    std::unique_ptr<ClipboardHistoryControllerDelegate> delegate)
    : delegate_(std::move(delegate)),
      image_model_factory_(delegate_->CreateImageModelFactory()),
      url_title_fetcher_(delegate_->CreateUrlTitleFetcher()),
      clipboard_history_(std::make_unique<ClipboardHistory>()),
      resource_manager_(std::make_unique<ClipboardHistoryResourceManager>(
          clipboard_history_.get())),
      accelerator_target_(std::make_unique<AcceleratorTarget>(this)),
      nudge_controller_(
          std::make_unique<ClipboardNudgeController>(clipboard_history_.get())),
      menu_delegate_(std::make_unique<MenuDelegate>(this)) {
  if (!image_model_factory_ || !url_title_fetcher_) {
    CHECK_IS_TEST();
  }
  clipboard_history_->AddObserver(this);
  resource_manager_->AddObserver(this);
  SessionController::Get()->AddObserver(this);
}

ClipboardHistoryControllerImpl::~ClipboardHistoryControllerImpl() {
  SessionController::Get()->RemoveObserver(this);
  resource_manager_->RemoveObserver(this);
  clipboard_history_->RemoveObserver(this);
}

// static
void ClipboardHistoryControllerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  ClipboardNudgeController::RegisterProfilePrefs(registry);
  registry->RegisterTimePref(prefs::kMultipasteMenuLastTimeShown, base::Time());
}

void ClipboardHistoryControllerImpl::Shutdown() {
  if (IsMenuShowing()) {
    context_menu_->Cancel(/*will_paste_item=*/false);
  }
  nudge_controller_.reset();
}

bool ClipboardHistoryControllerImpl::IsMenuShowing() const {
  return context_menu_ && context_menu_->IsRunning();
}

void ClipboardHistoryControllerImpl::ToggleMenuShownByAccelerator(
    bool is_plain_text_paste) {
  if (IsMenuShowing()) {
    // Before hiding the menu, paste the selected menu item, or the first item
    // if none is selected.
    PasteClipboardItemByCommandId(
        context_menu_->GetSelectedMenuItemCommand().value_or(
            clipboard_history_util::kFirstItemCommandId),
        is_plain_text_paste ? ClipboardHistoryPasteType::kPlainTextAccelerator
                            : ClipboardHistoryPasteType::kRichTextAccelerator);
    return;
  }

  // Do not allow the plain text shortcut to open the menu.
  if (is_plain_text_paste) {
    return;
  }

  if (clipboard_history_util::IsEnabledInCurrentMode() && IsEmpty()) {
    nudge_controller_->ShowNudge(ClipboardNudgeType::kZeroStateNudge);
    return;
  }

  ShowMenu(CalculateAnchorRect(), ui::MENU_SOURCE_KEYBOARD,
           crosapi::mojom::ClipboardHistoryControllerShowSource::kAccelerator);
}

void ClipboardHistoryControllerImpl::AddObserver(
    ClipboardHistoryController::Observer* observer) {
  observers_.AddObserver(observer);
}

void ClipboardHistoryControllerImpl::RemoveObserver(
    ClipboardHistoryController::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool ClipboardHistoryControllerImpl::ShowMenu(
    const gfx::Rect& anchor_rect,
    ui::MenuSourceType source_type,
    crosapi::mojom::ClipboardHistoryControllerShowSource show_source) {
  return ShowMenu(anchor_rect, source_type, show_source,
                  OnMenuClosingCallback());
}

bool ClipboardHistoryControllerImpl::ShowMenu(
    const gfx::Rect& anchor_rect,
    ui::MenuSourceType source_type,
    crosapi::mojom::ClipboardHistoryControllerShowSource show_source,
    OnMenuClosingCallback callback) {
  if (IsMenuShowing() || !HasAvailableHistoryItems()) {
    return false;
  }

  // Close the running context menu, if any, before showing the clipboard
  // history menu.
  if (auto* active_menu_instance = views::MenuController::GetActiveInstance()) {
    active_menu_instance->Cancel(views::MenuController::ExitType::kAll);
  }

  last_menu_source_ = show_source;

  // `Unretained()` is safe because `this` owns `context_menu_`.
  context_menu_ = ClipboardHistoryMenuModelAdapter::Create(
      menu_delegate_.get(), std::move(callback),
      base::BindRepeating(&ClipboardHistoryControllerImpl::OnMenuClosed,
                          base::Unretained(this)),
      clipboard_history_.get());
  context_menu_->Run(anchor_rect, source_type, show_source,
                     GetMenuLastTimeShown(),
                     nudge_controller_->GetNudgeLastTimeShown());

  CHECK(IsMenuShowing());
  accelerator_target_->OnMenuShown();

  // The first menu item should be selected by default after the clipboard
  // history menu shows. Note that the menu item is selected asynchronously
  // to avoid the interference from synthesized mouse events.
  menu_task_timer_.Start(
      FROM_HERE, base::TimeDelta(),
      base::BindOnce(
          [](const base::WeakPtr<ClipboardHistoryControllerImpl>&
                 controller_weak_ptr) {
            if (!controller_weak_ptr) {
              return;
            }

            controller_weak_ptr->context_menu_->SelectMenuItemWithCommandId(
                clipboard_history_util::kFirstItemCommandId);
            if (controller_weak_ptr->initial_item_selected_callback_for_test_) {
              controller_weak_ptr->initial_item_selected_callback_for_test_
                  .Run();
            }
          },
          weak_ptr_factory_.GetWeakPtr()));

  MarkMenuLastTimeShown();
  base::UmaHistogramEnumeration("Ash.ClipboardHistory.ContextMenu.ShowMenu",
                                show_source);

  for (auto& observer : observers_) {
    observer.OnClipboardHistoryMenuShown(show_source);
  }
  return true;
}

bool ClipboardHistoryControllerImpl::IsEmpty() const {
  return clipboard_history_->IsEmpty();
}

void ClipboardHistoryControllerImpl::FireItemUpdateNotificationTimerForTest() {
  item_update_notification_timer_.FireNow();
}

void ClipboardHistoryControllerImpl::GetHistoryValues(
    GetHistoryValuesCallback callback) const {
  // Map of `ClipboardHistoryItem` IDs to their corresponding bitmaps.
  std::map<base::UnguessableToken, SkBitmap> bitmaps_to_be_encoded;
  for (auto& item : clipboard_history_->GetItems()) {
    if (item.display_format() ==
        crosapi::mojom::ClipboardHistoryDisplayFormat::kPng) {
      const auto& maybe_png = item.data().maybe_png();
      if (!maybe_png.has_value()) {
        // The clipboard contains an image which has not yet been encoded to a
        // PNG.
        auto maybe_bitmap = item.data().GetBitmapIfPngNotEncoded();
        DCHECK(maybe_bitmap.has_value());
        bitmaps_to_be_encoded.emplace(item.id(),
                                      std::move(maybe_bitmap.value()));
      }
    }
  }

  // Map of `ClipboardHistoryItem` IDs to their encoded PNGs.
  auto encoded_pngs = std::make_unique<
      std::map<base::UnguessableToken, std::vector<uint8_t>>>();
  auto* encoded_pngs_ptr = encoded_pngs.get();

  // Post back to this sequence once all images have been encoded.
  base::RepeatingClosure barrier = base::BarrierClosure(
      bitmaps_to_be_encoded.size(),
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &ClipboardHistoryControllerImpl::GetHistoryValuesWithEncodedPNGs,
          weak_ptr_factory_.GetMutableWeakPtr(), std::move(callback),
          std::move(encoded_pngs))));

  // Encode images on background threads.
  for (auto id_and_bitmap : bitmaps_to_be_encoded) {
    base::ThreadPool::PostTask(
        FROM_HERE, base::BindOnce(&EncodeBitmapToPNG, barrier, encoded_pngs_ptr,
                                  std::move(id_and_bitmap.first),
                                  std::move(id_and_bitmap.second)));
  }

  if (!new_bitmap_to_write_while_encoding_for_test_.isNull()) {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteImage(new_bitmap_to_write_while_encoding_for_test_);
    new_bitmap_to_write_while_encoding_for_test_.reset();
  }
}

gfx::Rect ClipboardHistoryControllerImpl::GetMenuBoundsInScreenForTest() const {
  return context_menu_->GetMenuBoundsInScreenForTest();  // IN-TEST
}

void ClipboardHistoryControllerImpl::BlockGetHistoryValuesForTest() {
  get_history_values_blocker_for_test_.reset();
  get_history_values_blocker_for_test_ = std::make_unique<base::OneShotEvent>();
}

void ClipboardHistoryControllerImpl::ResumeGetHistoryValuesForTest() {
  DCHECK(get_history_values_blocker_for_test_);
  get_history_values_blocker_for_test_->Signal();
}

void ClipboardHistoryControllerImpl::OnScreenshotNotificationCreated() {
  nudge_controller_->MarkScreenshotNotificationShown();
}

bool ClipboardHistoryControllerImpl::HasAvailableHistoryItems() const {
  return clipboard_history_util::IsEnabledInCurrentMode() && !IsEmpty();
}

std::unique_ptr<ScopedClipboardHistoryPause>
ClipboardHistoryControllerImpl::CreateScopedPause() {
  return std::make_unique<ScopedClipboardHistoryPauseImpl>(
      clipboard_history_.get());
}

void ClipboardHistoryControllerImpl::GetHistoryValuesWithEncodedPNGs(
    GetHistoryValuesCallback callback,
    std::unique_ptr<std::map<base::UnguessableToken, std::vector<uint8_t>>>
        encoded_pngs) {
  // If a test is performing some work that must be done before history values
  // are returned, wait to run this function until that work is finished.
  if (get_history_values_blocker_for_test_ &&
      !get_history_values_blocker_for_test_->is_signaled()) {
    get_history_values_blocker_for_test_->Post(
        FROM_HERE,
        base::BindOnce(
            &ClipboardHistoryControllerImpl::GetHistoryValuesWithEncodedPNGs,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback),
            std::move(encoded_pngs)));
    return;
  }

  std::vector<ClipboardHistoryItem> item_results;

  // Check after asynchronous PNG encoding finishes to make sure we have not
  // entered a state where clipboard history is disabled, e.g., a locked screen.
  if (!clipboard_history_util::IsEnabledInCurrentMode()) {
    std::move(callback).Run(std::move(item_results));
    return;
  }

  bool all_images_encoded = true;
  for (auto& item : clipboard_history_->GetItems()) {
    if (item.display_format() ==
            crosapi::mojom::ClipboardHistoryDisplayFormat::kPng &&
        !item.data().maybe_png().has_value()) {
      // The clipboard contains an image which has not yet been encoded to a
      // PNG. Hopefully we just finished encoding and the PNG can be found
      // in `encoded_pngs`; otherwise this item was added while other PNGs
      // were being encoded.
      auto png_it = encoded_pngs->find(item.id());
      if (png_it == encoded_pngs->end()) {
        // Can't find the encoded PNG. We'll need to restart
        // `GetHistoryValues()` from the top, but allow this for loop to finish
        // to let PNGs we've already encoded get set to their appropriate
        // clipboards, to avoid re-encoding.
        all_images_encoded = false;
      } else {
        item.data().SetPngDataAfterEncoding(std::move(png_it->second));
      }
    }

    item_results.emplace_back(item);
  }

  if (!all_images_encoded) {
    GetHistoryValues(std::move(callback));
    return;
  }

  std::move(callback).Run(std::move(item_results));
}

std::vector<std::string> ClipboardHistoryControllerImpl::GetHistoryItemIds()
    const {
  std::vector<std::string> item_ids;
  if (HasAvailableHistoryItems()) {
    for (const auto& item : history()->GetItems()) {
      item_ids.push_back(item.id().ToString());
    }
  }
  return item_ids;
}

bool ClipboardHistoryControllerImpl::PasteClipboardItemById(
    const std::string& item_id,
    int event_flags,
    crosapi::mojom::ClipboardHistoryControllerShowSource paste_source) {
  const std::list<ClipboardHistoryItem>& history_items = history()->GetItems();
  auto iter_by_id = std::find_if(history_items.cbegin(), history_items.cend(),
                                 [&item_id](const ClipboardHistoryItem& item) {
                                   return item.id().ToString() == item_id;
                                 });
  if (iter_by_id == history_items.cend()) {
    return false;
  }

  RecordPasteItemIndex(std::distance(history_items.cbegin(), iter_by_id) +
                       clipboard_history_util::kFirstItemCommandId);

  MaybePostPasteTask(*iter_by_id, CalculatePasteType(paste_source, event_flags),
                     paste_source);
  return true;
}

bool ClipboardHistoryControllerImpl::DeleteClipboardItemById(
    const std::string& item_id) {
  for (const auto& item : history()->GetItems()) {
    if (item.id().ToString() == item_id) {
      DeleteClipboardHistoryItem(item);
      return true;
    }
  }
  return false;
}

void ClipboardHistoryControllerImpl::OnClipboardHistoryItemAdded(
    const ClipboardHistoryItem& item,
    bool is_duplicate) {
  PostItemUpdateNotificationTask();
}

void ClipboardHistoryControllerImpl::OnClipboardHistoryItemRemoved(
    const ClipboardHistoryItem& item) {
  PostItemUpdateNotificationTask();
}

void ClipboardHistoryControllerImpl::OnClipboardHistoryCleared() {
  // Prevent clipboard contents from being restored if the clipboard history is
  // cleared shortly after pasting an item.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Notify observers of the history being cleared after invalidating weak
  // pointers.
  PostItemUpdateNotificationTask();

  // Make sure the menu is closed now that there are no items to show.
  if (IsMenuShowing()) {
    context_menu_->Cancel(/*will_paste_item=*/false);
  }
}

void ClipboardHistoryControllerImpl::OnOperationConfirmed(bool copy) {
  static int confirmed_paste_count = 0;

  // Here we assume that a paste operation from the clipboard history menu never
  // interleaves with a user-initiated copy or paste operation from another
  // source, such as pressing the ctrl-v accelerator or clicking a context menu
  // option. In other words, when `pastes_to_be_confirmed_` is positive, the
  // next confirmed operation is expected to be a paste from clipboard history.
  // This assumption should hold in most cases given that the clipboard history
  // menu is always closed after one paste, and it usually takes a relatively
  // long time for a user to perform the next copy or paste. For this metric, we
  // tolerate a small margin of error.
  if (pastes_to_be_confirmed_ > 0 && !copy) {
    ++confirmed_paste_count;
    --pastes_to_be_confirmed_;
  } else {
    // Note that both copies and pastes from the standard clipboard cause the
    // clipboard history consecutive paste count to be emitted and reset.
    if (confirmed_paste_count > 0) {
      base::UmaHistogramCounts100("Ash.ClipboardHistory.ConsecutivePastes",
                                  confirmed_paste_count);
      confirmed_paste_count = 0;
    }

    if (copy) {
      // Record copy actions once they are confirmed, rather than when clipboard
      // data first changes, to allow multiple data changes to be debounced into
      // a single copy operation. This ensures that each user-initiated copy is
      // recorded only once. See `ClipboardHistory::OnDataChanged()` for further
      // explanation.
      base::RecordAction(base::UserMetricsAction("Ash_Clipboard_CopiedItem"));
    } else {
      // Pastes from clipboard history are already recorded in
      // `PasteMenuItemData()`. Here, we record just pastes from the standard
      // clipboard, to see how standard clipboard pastes interleave with
      // clipboard history pastes.
      base::RecordAction(base::UserMetricsAction("Ash_Clipboard_PastedItem"));
    }

    // Verify that this operation did not interleave with a clipboard history
    // paste.
    DCHECK_EQ(pastes_to_be_confirmed_, 0);
    // Whether or not the non-interleaving assumption has held, always reset
    // `pastes_to_be_confirmed_` to prevent standard clipboard pastes from
    // possibly being counted as clipboard history pastes, which could
    // significantly affect the clipboard history consecutive pastes metric.
    pastes_to_be_confirmed_ = 0;
  }

  // Callback will be run after clipboard data restoration.
  if (confirmed_operation_callback_for_test_ && !clipboard_data_replaced_) {
    confirmed_operation_callback_for_test_.Run(/*success=*/true);
  }
}

void ClipboardHistoryControllerImpl::OnCachedImageModelUpdated(
    const std::vector<base::UnguessableToken>& menu_item_ids) {
  PostItemUpdateNotificationTask();
}

void ClipboardHistoryControllerImpl::OnSessionStateChanged(
    session_manager::SessionState state) {
  PostItemUpdateNotificationTask();
}

void ClipboardHistoryControllerImpl::OnLoginStatusChanged(
    LoginStatus login_status) {
  PostItemUpdateNotificationTask();
}

void ClipboardHistoryControllerImpl::PostItemUpdateNotificationTask() {
  // Uses the async task to debounce multiple clipboard history changes in
  // short duration. Restart the timer if it is running.
  // This is done to avoid notifying observers multiple times if there are
  // multiple clipboard history changes in a short period. For example, if the
  // clipboard history reaches the cache limit and a new clipboard history item
  // arrives at the same time, there would be two clipboard history changes: the
  // addition of the new item and the removal of an obsolete item. In this case,
  // this class should only notify observers only once.
  item_update_notification_timer_.Start(
      FROM_HERE, base::TimeDelta(),
      base::BindOnce(
          &ClipboardHistoryControllerImpl::MaybeNotifyObserversOfItemUpdate,
          weak_ptr_factory_.GetWeakPtr()));
}

void ClipboardHistoryControllerImpl::MaybeNotifyObserversOfItemUpdate() {
  const bool has_available_items = HasAvailableHistoryItems();
  if (!has_available_items && !has_available_items_in_last_update_) {
    // There are no available items, and there were none in the last
    // notification either. Nothing has changed, so return early.
    return;
  }

  for (auto& observer : observers_) {
    observer.OnClipboardHistoryItemsUpdated();
  }
  has_available_items_in_last_update_ = has_available_items;
}

void ClipboardHistoryControllerImpl::ExecuteCommand(int command_id,
                                                    int event_flags) {
  DCHECK(context_menu_);

  DCHECK_GE(command_id, clipboard_history_util::kFirstItemCommandId);
  DCHECK_LE(command_id, clipboard_history_util::kMaxItemCommandId);

  using Action = clipboard_history_util::Action;
  Action action = context_menu_->GetActionForCommandId(command_id);
  switch (action) {
    case Action::kPaste:
      PasteClipboardItemByCommandId(
          command_id, CalculatePasteType(last_menu_source_, event_flags));
      return;
    case Action::kDelete:
      DeleteItemWithCommandId(command_id);
      return;
    case Action::kSelect:
      context_menu_->SelectMenuItemWithCommandId(command_id);
      return;
    case Action::kSelectItemHoveredByMouse:
      context_menu_->SelectMenuItemHoveredByMouse();
      return;
    case Action::kEmpty:
      DUMP_WILL_BE_NOTREACHED();
      return;
  }
}

void ClipboardHistoryControllerImpl::PasteClipboardItemByCommandId(
    int command_id,
    ClipboardHistoryPasteType paste_type) {
  // Force close the context menu. Failure to do so before dispatching our
  // synthetic key event will result in the context menu consuming the event.
  // When closing the menu, indicate that the menu is closing because of an
  // imminent paste. Note that in some cases, this will indicate paste intent
  // for pastes that ultimately fail. For now, this is an acceptable inaccuracy.
  CHECK(context_menu_);
  context_menu_->Cancel(/*will_paste_item=*/true);

  // `command_id` should match the pasted item's index in `context_menu_`.
  RecordPasteItemIndex(command_id);

  MaybePostPasteTask(context_menu_->GetItemFromCommandId(command_id),
                     paste_type, last_menu_source_);
}

void ClipboardHistoryControllerImpl::MaybePostPasteTask(
    const ClipboardHistoryItem& item,
    ClipboardHistoryPasteType paste_type,
    crosapi::mojom::ClipboardHistoryControllerShowSource paste_source) {
  // Deactivate ClipboardImageModelFactory prior to pasting to ensure that any
  // modifications to the clipboard for HTML rendering purposes are reversed.
  // This factory may be nullptr in tests.
  if (auto* clipboard_image_factory = ClipboardImageModelFactory::Get()) {
    clipboard_image_factory->Deactivate();
  }

  if (auto* active_window = window_util::GetActiveWindow()) {
    // Paste asynchronously to ensure ARC windows handle paste events correctly.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ClipboardHistoryControllerImpl::PasteClipboardHistoryItem,
            weak_ptr_factory_.GetWeakPtr(), active_window, item, paste_type,
            paste_source));
  }
}

void ClipboardHistoryControllerImpl::PasteClipboardHistoryItem(
    aura::Window* intended_window,
    ClipboardHistoryItem item,
    ClipboardHistoryPasteType paste_type,
    crosapi::mojom::ClipboardHistoryControllerShowSource paste_source) {
  // Return early if any of these conditions occur:
  // 1. The original clipboard data has been replaced by an in-progress
  //    clipboard history paste.
  // 2. The active window has changed.
  // 3. The clipboard history feature is disabled under the current mode.
  if (clipboard_data_replaced_ || !intended_window ||
      intended_window != window_util::GetActiveWindow() ||
      !clipboard_history_util::IsEnabledInCurrentMode()) {
    if (confirmed_operation_callback_for_test_)
      confirmed_operation_callback_for_test_.Run(/*success=*/false);
    return;
  }

  // Get information about the data to be pasted.
  bool paste_plain_text = IsPlainTextPaste(paste_type);
  auto* clipboard = GetClipboard();
  ui::DataTransferEndpoint data_dst(ui::EndpointType::kClipboardHistory);
  const auto* current_clipboard_data = clipboard->GetClipboardData(&data_dst);

  // Clipboard history pastes are performed by temporarily writing data to the
  // system clipboard, if necessary, and then issuing a standard paste.
  // Determine the data we should temporarily write to the clipboard, if any, so
  // that we can paste the selected history item.
  std::unique_ptr<ui::ClipboardData> data_to_paste;
  if (paste_plain_text) {
    data_to_paste = std::make_unique<ui::ClipboardData>();
    data_to_paste->set_commit_time(item.data().commit_time());
    data_to_paste->set_text(item.data().text());
    auto data_src = item.data().source();
    if (data_src) {
      data_to_paste->set_source(data_src);
    }
  } else if (!current_clipboard_data ||
             *current_clipboard_data != item.data()) {
    data_to_paste = std::make_unique<ui::ClipboardData>(item.data());
  }

  // Pause changes to clipboard history while manipulating the clipboard.
  std::unique_ptr<ui::ClipboardData> replaced_data;
  // If necessary, replace the clipboard's current data before issuing a paste.
  if (data_to_paste) {
    ScopedClipboardHistoryPauseImpl scoped_pause(
        clipboard_history_.get(),
        clipboard_history_util::PauseBehavior::kDefault);
    replaced_data =
        GetClipboard()->WriteClipboardData(std::move(data_to_paste));
    clipboard_data_replaced_ = !!replaced_data;
  }

  ++pastes_to_be_confirmed_;

  // Use synthetic pastes as a fallback solution.
  if (!delegate_->Paste()) {
    SyntheticPaste(paste_source);
  }

  clipboard_history_util::RecordClipboardHistoryItemPasted(item);
  base::UmaHistogramEnumeration("Ash.ClipboardHistory.PasteType", paste_type);
  base::UmaHistogramEnumeration("Ash.ClipboardHistory.PasteSource",
                                paste_source);

  for (auto& observer : observers_) {
    observer.OnClipboardHistoryPasted();
  }

  // If the clipboard was not changed--i.e., we pasted the full data on the
  // clipboard--then we are done modifying the clipboard buffer.
  if (!replaced_data) {
    return;
  }

  // Restore the clipboard data asynchronously. Some apps take a long time to
  // receive the paste event, and some apps will read from the clipboard
  // multiple times per paste. Wait a bit before writing `data_to_restore` back
  // to the clipboard.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](const base::WeakPtr<ClipboardHistoryControllerImpl>& weak_ptr,
             std::unique_ptr<ui::ClipboardData> data_to_restore) {
            std::unique_ptr<ScopedClipboardHistoryPauseImpl> scoped_pause;
            if (weak_ptr) {
              // When restoring the original clipboard content, pause clipboard
              // history to avoid committing data already at the top of the
              // clipboard history list.
              scoped_pause = std::make_unique<ScopedClipboardHistoryPauseImpl>(
                  weak_ptr->clipboard_history_.get(),
                  clipboard_history_util::PauseBehavior::kDefault);
            }
            GetClipboard()->WriteClipboardData(std::move(data_to_restore));

            if (weak_ptr) {
              weak_ptr->clipboard_data_replaced_ = false;

              // Confirm the operation after data restoration if needed.
              if (auto& callback_for_test =
                      weak_ptr->confirmed_operation_callback_for_test_;
                  callback_for_test && !weak_ptr->pastes_to_be_confirmed_) {
                callback_for_test.Run(/*success=*/true);
              }
            }
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(replaced_data)),
      buffer_restoration_delay_for_test_.value_or(base::Milliseconds(200)));
}

void ClipboardHistoryControllerImpl::DeleteSelectedMenuItemIfAny() {
  DCHECK(context_menu_);
  auto selected_command = context_menu_->GetSelectedMenuItemCommand();

  // Return early if no item is selected.
  if (!selected_command.has_value())
    return;

  DeleteItemWithCommandId(*selected_command);
}

void ClipboardHistoryControllerImpl::DeleteItemWithCommandId(int command_id) {
  DCHECK(context_menu_);

  // Pressing VKEY_DELETE is handled here via AcceleratorTarget because the
  // contextual menu consumes the key event. Record the "pressing the delete
  // button" histogram here because this action does the same thing as
  // activating the button directly via click/tap. There is no special handling
  // for pasting an item via VKEY_RETURN because in that case the menu does not
  // process the key event.
  const auto& to_be_deleted_item =
      context_menu_->GetItemFromCommandId(command_id);
  DeleteClipboardHistoryItem(to_be_deleted_item);

  // If the item to be deleted is the last one, close the whole menu.
  if (context_menu_->GetMenuItemsCount() == 1) {
    context_menu_->Cancel(/*will_paste_item=*/false);
    return;
  }

  context_menu_->RemoveMenuItemWithCommandId(command_id);
}

void ClipboardHistoryControllerImpl::DeleteClipboardHistoryItem(
    const ClipboardHistoryItem& item) {
  clipboard_history_util::RecordClipboardHistoryItemDeleted(item);
  clipboard_history_->RemoveItemForId(item.id());
}

void ClipboardHistoryControllerImpl::AdvancePseudoFocus(bool reverse) {
  DCHECK(context_menu_);
  context_menu_->AdvancePseudoFocus(reverse);
}

gfx::Rect ClipboardHistoryControllerImpl::CalculateAnchorRect() const {
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  auto* host = GetWindowTreeHostForDisplay(display.id());

  // Some web apps render the caret in an IFrame, and we will not get the
  // bounds in that case.
  // TODO(crbug.com/40137728): Show the menu in the middle of the
  // webview if the bounds are empty.
  ui::TextInputClient* text_input_client =
      host->GetInputMethod()->GetTextInputClient();

  // `text_input_client` may be null. For example, in clamshell mode and without
  // any window open.
  const gfx::Rect textfield_bounds =
      text_input_client ? text_input_client->GetCaretBounds() : gfx::Rect();

  // Note that the width of caret's bounds may be zero in some views (such as
  // the search bar of Google search web page). So we cannot use
  // gfx::Size::IsEmpty() here. In addition, the applications using IFrame may
  // provide unreliable `textfield_bounds` which are not fully contained by the
  // display bounds.
  const bool textfield_bounds_are_valid =
      textfield_bounds.size() != gfx::Size() &&
      IsRectContainedByAnyDisplay(textfield_bounds);

  if (textfield_bounds_are_valid)
    return textfield_bounds;

  return gfx::Rect(display::Screen::GetScreen()->GetCursorScreenPoint(),
                   gfx::Size());
}

void ClipboardHistoryControllerImpl::OnMenuClosed() {
  accelerator_target_->OnMenuClosed();

  // Reset `context_menu_` in the asynchronous way. Because the menu may be
  // accessed after `OnMenuClosed()` is called.
  menu_task_timer_.Start(
      FROM_HERE, base::TimeDelta(),
      base::BindOnce(
          [](const base::WeakPtr<ClipboardHistoryControllerImpl>&
                 controller_weak_ptr) {
            if (controller_weak_ptr)
              controller_weak_ptr->context_menu_.reset();
          },
          weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace ash
