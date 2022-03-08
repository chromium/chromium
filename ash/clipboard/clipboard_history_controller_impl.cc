// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_controller_impl.h"

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/clipboard/clipboard_history_menu_model_adapter.h"
#include "ash/clipboard/clipboard_history_resource_manager.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/clipboard_nudge_constants.h"
#include "ash/clipboard/clipboard_nudge_controller.h"
#include "ash/clipboard/scoped_clipboard_history_pause_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/display/display_util.h"
#include "ash/public/cpp/clipboard_image_model_factory.h"
#include "ash/public/cpp/style/scoped_light_mode_as_default.h"
#include "ash/public/cpp/window_tree_host_lookup.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/json/values_util.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/menu/menu_controller.h"

namespace ash {

namespace {

constexpr char kImageDataKey[] = "imageData";
constexpr char kTextDataKey[] = "textData";
constexpr char kFormatDataKey[] = "displayFormat";

constexpr char kPngFormat[] = "png";
constexpr char kHtmlFormat[] = "html";
constexpr char kTextFormat[] = "text";
constexpr char kFileFormat[] = "file";

ui::ClipboardNonBacked* GetClipboard() {
  auto* clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
  DCHECK(clipboard);
  return clipboard;
}

// Encodes `bitmap` and maps the corresponding ClipboardHistoryItem ID, `id, to
// the resulting PNG in `encoded_pngs`. This function should run on a background
// thread.
void EncodeBitmapToPNG(
    base::OnceClosure barrier_callback,
    std::map<base::UnguessableToken, std::vector<uint8_t>>* const encoded_pngs,
    base::UnguessableToken id,
    SkBitmap bitmap) {
  auto png = ui::ClipboardData::EncodeBitmapData(bitmap);

  // Don't acquire the lock until after the image encoding has finished.
  static base::NoDestructor<base::Lock> map_lock;
  base::AutoLock lock(*map_lock);

  encoded_pngs->emplace(id, std::move(png));
  std::move(barrier_callback).Run();
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
            /*key_state=*/ui::Accelerator::KeyState::PRESSED)) {}
  AcceleratorTarget(const AcceleratorTarget&) = delete;
  AcceleratorTarget& operator=(const AcceleratorTarget&) = delete;
  ~AcceleratorTarget() override = default;

  void OnMenuShown() {
    Shell::Get()->accelerator_controller()->Register(
        {delete_selected_, tab_navigation_, shift_tab_navigation_},
        /*accelerator_target=*/this);
  }

  void OnMenuClosed() {
    Shell::Get()->accelerator_controller()->Unregister(
        delete_selected_, /*accelerator_target=*/this);
    Shell::Get()->accelerator_controller()->Unregister(
        tab_navigation_, /*accelerator_target=*/this);
    Shell::Get()->accelerator_controller()->Unregister(
        shift_tab_navigation_, /*accelerator_target=*/this);
  }

 private:
  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    if (accelerator == delete_selected_) {
      HandleDeleteSelected(accelerator.modifiers());
    } else if (accelerator == tab_navigation_) {
      HandleTab();
    } else if (accelerator == shift_tab_navigation_) {
      HandleShiftTab();
    } else {
      NOTREACHED();
      return false;
    }

    return true;
  }

  bool CanHandleAccelerators() const override {
    return controller_->IsMenuShowing() || controller_->CanShowMenu();
  }

  void HandleDeleteSelected(int event_flags) {
    DCHECK(controller_->IsMenuShowing());
    controller_->DeleteSelectedMenuItemIfAny();
  }

  void HandleTab() {
    DCHECK(controller_->IsMenuShowing());
    controller_->AdvancePseudoFocus(/*reverse=*/false);
  }

  void HandleShiftTab() {
    DCHECK(controller_->IsMenuShowing());
    controller_->AdvancePseudoFocus(/*reverse=*/true);
  }

  // The controller responsible for showing the Clipboard History menu.
  ClipboardHistoryControllerImpl* const controller_;

  // The accelerator to delete the selected menu item. It is only registered
  // while the menu is showing.
  const ui::Accelerator delete_selected_;

  // Move the pseudo focus forward.
  const ui::Accelerator tab_navigation_;

  // Moves the pseudo focus backward.
  const ui::Accelerator shift_tab_navigation_;
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
  ClipboardHistoryControllerImpl* const controller_;
};

// ClipboardHistoryControllerImpl ----------------------------------------------

ClipboardHistoryControllerImpl::ClipboardHistoryControllerImpl()
    : clipboard_history_(std::make_unique<ClipboardHistory>()),
      resource_manager_(std::make_unique<ClipboardHistoryResourceManager>(
          clipboard_history_.get())),
      accelerator_target_(std::make_unique<AcceleratorTarget>(this)),
      menu_delegate_(std::make_unique<MenuDelegate>(this)),
      nudge_controller_(
          std::make_unique<ClipboardNudgeController>(clipboard_history_.get(),
                                                     this)) {
  clipboard_history_->AddObserver(this);
  resource_manager_->AddObserver(this);
}

ClipboardHistoryControllerImpl::~ClipboardHistoryControllerImpl() {
  resource_manager_->RemoveObserver(this);
  clipboard_history_->RemoveObserver(this);
}

void ClipboardHistoryControllerImpl::Shutdown() {
  nudge_controller_.reset();
}

void ClipboardHistoryControllerImpl::AddObserver(
    ClipboardHistoryController::Observer* observer) const {
  observers_.AddObserver(observer);
}

void ClipboardHistoryControllerImpl::RemoveObserver(
    ClipboardHistoryController::Observer* observer) const {
  observers_.RemoveObserver(observer);
}

bool ClipboardHistoryControllerImpl::IsMenuShowing() const {
  return context_menu_ && context_menu_->IsRunning();
}

void ClipboardHistoryControllerImpl::ShowMenuByAccelerator() {
  if (IsMenuShowing()) {
    ExecuteSelectedMenuItem(ui::EF_COMMAND_DOWN);
    return;
  }

  if (ClipboardHistoryUtil::IsEnabledInCurrentMode() && IsEmpty()) {
    nudge_controller_->ShowNudge(ClipboardNudgeType::kZeroStateNudge);
    return;
  }

  ShowMenu(CalculateAnchorRect(), ui::MENU_SOURCE_KEYBOARD,
           crosapi::mojom::ClipboardHistoryControllerShowSource::kAccelerator);
}

void ClipboardHistoryControllerImpl::ShowMenu(
    const gfx::Rect& anchor_rect,
    ui::MenuSourceType source_type,
    crosapi::mojom::ClipboardHistoryControllerShowSource show_source) {
  if (IsMenuShowing() || !CanShowMenu())
    return;

  // Close the running context menu if any before showing the clipboard history
  // menu. Because the clipboard history menu should not be nested.
  auto* active_menu_instance = views::MenuController::GetActiveInstance();
  if (active_menu_instance)
    active_menu_instance->Cancel(views::MenuController::ExitType::kAll);

  context_menu_ = ClipboardHistoryMenuModelAdapter::Create(
      menu_delegate_.get(),
      base::BindRepeating(&ClipboardHistoryControllerImpl::OnMenuClosed,
                          base::Unretained(this)),
      clipboard_history_.get(), resource_manager_.get());
  context_menu_->Run(anchor_rect, source_type);

  DCHECK(IsMenuShowing());
  accelerator_target_->OnMenuShown();

  base::UmaHistogramEnumeration("Ash.ClipboardHistory.ContextMenu.ShowMenu",
                                show_source);

  // The first menu item should be selected as default after the clipboard
  // history menu shows. Note that the menu item is selected asynchronously
  // to avoid the interference from synthesized mouse events.
  menu_task_timer_.Start(
      FROM_HERE, base::TimeDelta(),
      base::BindOnce(
          [](const base::WeakPtr<ClipboardHistoryControllerImpl>&
                 controller_weak_ptr) {
            if (!controller_weak_ptr)
              return;

            controller_weak_ptr->context_menu_->SelectMenuItemWithCommandId(
                ClipboardHistoryUtil::kFirstItemCommandId);
            if (controller_weak_ptr->initial_item_selected_callback_for_test_) {
              controller_weak_ptr->initial_item_selected_callback_for_test_
                  .Run();
            }
          },
          weak_ptr_factory_.GetWeakPtr()));

  for (auto& observer : observers_)
    observer.OnClipboardHistoryMenuShown(show_source);
}

gfx::Rect ClipboardHistoryControllerImpl::GetMenuBoundsInScreenForTest() const {
  return context_menu_->GetMenuBoundsInScreenForTest();
}

void ClipboardHistoryControllerImpl::GetHistoryValuesForTest(
    GetHistoryValuesCallback callback) const {
  GetHistoryValues(/*item_id_filter=*/std::set<std::string>(),
                   std::move(callback));
}

bool ClipboardHistoryControllerImpl::ShouldShowNewFeatureBadge() const {
  return chromeos::features::IsClipboardHistoryContextMenuNudgeEnabled() &&
         nudge_controller_->ShouldShowNewFeatureBadge();
}

void ClipboardHistoryControllerImpl::MarkNewFeatureBadgeShown() {
  nudge_controller_->MarkNewFeatureBadgeShown();
}

void ClipboardHistoryControllerImpl::OnScreenshotNotificationCreated() {
  nudge_controller_->MarkScreenshotNotificationShown();
}

bool ClipboardHistoryControllerImpl::CanShowMenu() const {
  return !IsEmpty() && ClipboardHistoryUtil::IsEnabledInCurrentMode();
}

bool ClipboardHistoryControllerImpl::IsEmpty() const {
  return clipboard_history_->IsEmpty();
}

std::unique_ptr<ScopedClipboardHistoryPause>
ClipboardHistoryControllerImpl::CreateScopedPause() {
  return std::make_unique<ScopedClipboardHistoryPauseImpl>(
      clipboard_history_.get());
}

void ClipboardHistoryControllerImpl::GetHistoryValues(
    const std::set<std::string>& item_id_filter,
    GetHistoryValuesCallback callback) const {
  // Map of ClipboardHistoryItem IDs to their corresponding bitmaps.
  std::map<base::UnguessableToken, SkBitmap> bitmaps_to_be_encoded;
  // Get the clipboard data for each clipboard history item.
  for (auto& item : clipboard_history_->GetItems()) {
    // If the |item_id_filter| contains values, then only return the clipboard
    // items included in it.
    if (!item_id_filter.empty() &&
        item_id_filter.find(item.id().ToString()) == item_id_filter.end()) {
      continue;
    }

    if (ash::ClipboardHistoryUtil::CalculateDisplayFormat(item.data()) ==
        ash::ClipboardHistoryUtil::ClipboardHistoryDisplayFormat::kPng) {
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

  // Map of ClipboardHistoryItem ID to its encoded PNG. Since encoding images
  // may happen on separate threads, a lock is used to ensure thread-safe
  // insertion into `encoded_pngs`.
  auto encoded_pngs = std::make_unique<
      std::map<base::UnguessableToken, std::vector<uint8_t>>>();
  auto* encoded_pngs_ptr = encoded_pngs.get();

  // Post back to this sequence once all images have been encoded.
  base::RepeatingClosure barrier = base::BarrierClosure(
      bitmaps_to_be_encoded.size(),
      base::BindPostTask(
          base::SequencedTaskRunnerHandle::Get(),
          base::BindOnce(
              &ClipboardHistoryControllerImpl::GetHistoryValuesWithEncodedPNGs,
              weak_ptr_factory_.GetWeakPtr(), item_id_filter,
              std::move(callback), std::move(encoded_pngs))));

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

void ClipboardHistoryControllerImpl::GetHistoryValuesWithEncodedPNGs(
    const std::set<std::string>& item_id_filter,
    GetHistoryValuesCallback callback,
    std::unique_ptr<std::map<base::UnguessableToken, std::vector<uint8_t>>>
        encoded_pngs) {
  base::Value item_results(base::Value::Type::LIST);
  DCHECK(encoded_pngs);

  bool all_images_encoded = true;
  // Get the clipboard data for each clipboard history item.
  for (auto& item : clipboard_history_->GetItems()) {
    // If the |item_id_filter| contains values, then only return the clipboard
    // items included in it.
    if (!item_id_filter.empty() &&
        item_id_filter.find(item.id().ToString()) == item_id_filter.end()) {
      continue;
    }

    base::Value item_value(base::Value::Type::DICTIONARY);
    switch (ash::ClipboardHistoryUtil::CalculateDisplayFormat(item.data())) {
      case ash::ClipboardHistoryUtil::ClipboardHistoryDisplayFormat::kPng: {
        if (!item.data().maybe_png().has_value()) {
          // The clipboard contains an image which has not yet been encoded to a
          // PNG. Hopefully we just finished encoding and the PNG can be found
          // in `encoded_pngs`, otherwise this item was added while other PNGs
          // were being encoded.
          auto png_it = encoded_pngs->find(item.id());
          if (png_it == encoded_pngs->end()) {
            // Can't find the encoded PNG. We'll need to restart
            // GetHistoryValues from the top, but allow this for loop to finish
            // to let PNGs we've already encoded get set to their appropriate
            // clipboards, to avoid re-encoding.
            all_images_encoded = false;
          } else {
            item.data().SetPngDataAfterEncoding(std::move(png_it->second));
          }
        }

        const auto& maybe_png = item.data().maybe_png();
        if (maybe_png.has_value()) {
          item_value.SetKey(kImageDataKey, base::Value(webui::GetPngDataUrl(
                                               maybe_png.value().data(),
                                               maybe_png.value().size())));
          item_value.SetKey(kFormatDataKey, base::Value(kPngFormat));
        }
        break;
      }
      case ash::ClipboardHistoryUtil::ClipboardHistoryDisplayFormat::kHtml: {
        const SkBitmap& bitmap =
            *(resource_manager_->GetImageModel(item).GetImage().ToSkBitmap());
        item_value.SetKey(kImageDataKey,
                          base::Value(webui::GetBitmapDataUrl(bitmap)));
        item_value.SetKey(kFormatDataKey, base::Value(kHtmlFormat));
        break;
      }
      case ash::ClipboardHistoryUtil::ClipboardHistoryDisplayFormat::kText:
        item_value.SetKey(kTextDataKey, base::Value(item.data().text()));
        item_value.SetKey(kFormatDataKey, base::Value(kTextFormat));
        break;
      case ash::ClipboardHistoryUtil::ClipboardHistoryDisplayFormat::kFile: {
        std::string file_name =
            base::UTF16ToUTF8(resource_manager_->GetLabel(item));
        item_value.SetKey(kTextDataKey, base::Value(file_name));
        ScopedLightModeAsDefault scoped_light_mode_as_default;
        std::string data_url = webui::GetBitmapDataUrl(
            *ClipboardHistoryUtil::GetIconForFileClipboardItem(item, file_name)
                 .bitmap());
        item_value.SetKey(kImageDataKey, base::Value(data_url));
        item_value.SetKey(kFormatDataKey, base::Value(kFileFormat));
        break;
      }
    }
    item_value.SetKey("id", base::Value(item.id().ToString()));
    item_value.SetKey("timeCopied",
                      base::Value(item.time_copied().ToJsTimeIgnoringNull()));
    item_results.Append(std::move(item_value));
  }

  if (!all_images_encoded) {
    GetHistoryValues(item_id_filter, std::move(callback));
    return;
  }

  std::move(callback).Run(std::move(item_results));
}

std::vector<std::string> ClipboardHistoryControllerImpl::GetHistoryItemIds()
    const {
  std::vector<std::string> item_ids;
  for (const auto& item : history()->GetItems()) {
    item_ids.push_back(item.id().ToString());
  }
  return item_ids;
}

bool ClipboardHistoryControllerImpl::PasteClipboardItemById(
    const std::string& item_id) {
  if (currently_pasting_)
    return false;

  auto* active_window = window_util::GetActiveWindow();
  if (!active_window)
    return false;

  for (const auto& item : history()->GetItems()) {
    if (item.id().ToString() == item_id) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &ClipboardHistoryControllerImpl::PasteClipboardHistoryItem,
              weak_ptr_factory_.GetWeakPtr(), active_window, item,
              /*paste_plain_text=*/false));
      return true;
    }
  }
  return false;
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
  for (auto& observer : observers_)
    observer.OnClipboardHistoryItemListAddedOrRemoved();
}

void ClipboardHistoryControllerImpl::OnClipboardHistoryItemRemoved(
    const ClipboardHistoryItem& item) {
  for (auto& observer : observers_)
    observer.OnClipboardHistoryItemListAddedOrRemoved();
}

void ClipboardHistoryControllerImpl::OnClipboardHistoryCleared() {
  // Prevent clipboard contents getting restored if the Clipboard is cleared
  // soon after a `PasteMenuItemData()`.
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (!IsMenuShowing())
    return;
  context_menu_->Cancel();
}

void ClipboardHistoryControllerImpl::OnOperationConfirmed(bool copy) {
  static int confirmed_paste_count = 0;

  // Here we assume that a paste operation from the clipboard history menu never
  // interleaves with a copy operation or a paste operation from other ways (
  // including pressing the ctrl-v accelerator or clicking a context menu
  // option). In other words, when `pastes_to_be_confirmed_` is positive, it
  // means that the incoming operation should be a paste from clipboard history.
  // It should be held in most cases given that the clipboard history menu is
  // always closed after one paste and it usually takes relatively long time for
  // a user to conduct the next copy or paste. For this metric, we are tolerable
  // of a small portion of erroneous recordings.

  // When `pastes_to_be_confirmed_` is positive, `copy` should be
  // false in most cases based on the assumption above. But theoretically
  // `copy` could be true.
  if (pastes_to_be_confirmed_ > 0 && !copy) {
    ++confirmed_paste_count;
    --pastes_to_be_confirmed_;
  } else {
    // Reset if the assumption is not held for some reasons.
    DCHECK_LE(0, pastes_to_be_confirmed_);
    if (pastes_to_be_confirmed_ > 0)
      pastes_to_be_confirmed_ = 0;

    DCHECK_LE(0, confirmed_paste_count);
    if (confirmed_paste_count) {
      base::UmaHistogramCounts100("Ash.ClipboardHistory.ConsecutivePastes",
                                  confirmed_paste_count);
      confirmed_paste_count = 0;
    }
  }

  if (confirmed_operation_callback_for_test_)
    confirmed_operation_callback_for_test_.Run();
}

void ClipboardHistoryControllerImpl::OnCachedImageModelUpdated(
    const std::vector<base::UnguessableToken>& menu_item_ids) {
  for (auto& observer : observers_)
    observer.OnClipboardHistoryItemsUpdated(menu_item_ids);
}

void ClipboardHistoryControllerImpl::ExecuteSelectedMenuItem(int event_flags) {
  DCHECK(IsMenuShowing());

  auto command = context_menu_->GetSelectedMenuItemCommand();

  // If no menu item is currently selected, we'll fallback to the first item.
  PasteMenuItemData(command.value_or(ClipboardHistoryUtil::kFirstItemCommandId),
                    event_flags);
}

void ClipboardHistoryControllerImpl::ExecuteCommand(int command_id,
                                                    int event_flags) {
  DCHECK(context_menu_);

  DCHECK_GE(command_id, ClipboardHistoryUtil::kFirstItemCommandId);
  DCHECK_LE(command_id, ClipboardHistoryUtil::kMaxItemCommandId);

  using Action = ClipboardHistoryUtil::Action;
  Action action = context_menu_->GetActionForCommandId(command_id);
  switch (action) {
    case Action::kPaste:
      PasteMenuItemData(command_id, event_flags & ui::EF_SHIFT_DOWN);
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
      NOTREACHED();
      return;
  }
}

void ClipboardHistoryControllerImpl::PasteMenuItemData(int command_id,
                                                       bool paste_plain_text) {
  UMA_HISTOGRAM_ENUMERATION(
      "Ash.ClipboardHistory.ContextMenu.MenuOptionSelected", command_id,
      ClipboardHistoryUtil::kMaxCommandId);

  // Deactivate ClipboardImageModelFactory prior to pasting to ensure that any
  // modifications to the clipboard for HTML rendering purposes are reversed.
  ClipboardImageModelFactory::Get()->Deactivate();

  // Force close the context menu. Failure to do so before dispatching our
  // synthetic key event will result in the context menu consuming the event.
  DCHECK(context_menu_);
  context_menu_->Cancel();

  auto* active_window = window_util::GetActiveWindow();
  if (!active_window)
    return;

  const ClipboardHistoryItem& selected_item =
      context_menu_->GetItemFromCommandId(command_id);

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClipboardHistoryControllerImpl::PasteClipboardHistoryItem,
                     weak_ptr_factory_.GetWeakPtr(), active_window,
                     selected_item, paste_plain_text));
}

void ClipboardHistoryControllerImpl::PasteClipboardHistoryItem(
    aura::Window* intended_window,
    ClipboardHistoryItem item,
    bool paste_plain_text) {
  // It's possible that the window could change after posting the
  // PasteClipboardHistoryItem task is scheduled.
  if (!intended_window || intended_window != window_util::GetActiveWindow())
    return;

  auto* clipboard = GetClipboard();
  std::unique_ptr<ui::ClipboardData> original_data;

  // If necessary, replace the clipboard's |original_data| temporarily so that
  // we can paste the selected history item.
  ui::DataTransferEndpoint data_dst(ui::EndpointType::kClipboardHistory);
  const auto* current_clipboard_data = clipboard->GetClipboardData(&data_dst);
  if (paste_plain_text || !current_clipboard_data ||
      *current_clipboard_data != item.data()) {
    std::unique_ptr<ui::ClipboardData> temp_data;
    if (paste_plain_text) {
      // When the shift key is pressed, we only paste plain text.
      temp_data = std::make_unique<ui::ClipboardData>();
      temp_data->set_text(item.data().text());
      ui::DataTransferEndpoint* data_src = item.data().source();
      if (data_src) {
        temp_data->set_source(
            std::make_unique<ui::DataTransferEndpoint>(*data_src));
      }
    } else {
      temp_data = std::make_unique<ui::ClipboardData>(item.data());
    }

    // Pause clipboard history when manipulating the clipboard for a paste.
    ScopedClipboardHistoryPauseImpl scoped_pause(clipboard_history_.get());
    original_data = clipboard->WriteClipboardData(std::move(temp_data));
  }

  auto* host = GetWindowTreeHostForDisplay(
      display::Screen::GetScreen()->GetDisplayForNewWindows().id());
  DCHECK(host);

  ui::KeyEvent ctrl_press(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL, ui::EF_NONE);
  host->DeliverEventToSink(&ctrl_press);

  ui::KeyEvent v_press(ui::ET_KEY_PRESSED, ui::VKEY_V, ui::EF_CONTROL_DOWN);
  host->DeliverEventToSink(&v_press);

  ui::KeyEvent v_release(ui::ET_KEY_RELEASED, ui::VKEY_V, ui::EF_CONTROL_DOWN);
  host->DeliverEventToSink(&v_release);

  ui::KeyEvent ctrl_release(ui::ET_KEY_RELEASED, ui::VKEY_CONTROL, ui::EF_NONE);
  host->DeliverEventToSink(&ctrl_release);

  ++pastes_to_be_confirmed_;

  for (auto& observer : observers_)
    observer.OnClipboardHistoryPasted();

  // `original_data` only exists if the clipboard was modified.
  if (!original_data)
    return;

  currently_pasting_ = true;

  // Replace the original item back on top of the clipboard. Some apps take a
  // long time to receive the paste event, also some apps will read from the
  // clipboard multiple times per paste. Wait a bit before replacing the item
  // back onto the clipboard.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](const base::WeakPtr<ClipboardHistoryControllerImpl>& weak_ptr,
             std::unique_ptr<ui::ClipboardData> original_data) {
            // When restoring the original item back on top of the clipboard we
            // need to pause clipboard history. Failure to do so will result in
            // the original item being re-recorded when this restoration step
            // should actually be opaque to the user.
            std::unique_ptr<ScopedClipboardHistoryPauseImpl> scoped_pause;
            if (weak_ptr) {
              weak_ptr->currently_pasting_ = false;
              scoped_pause = std::make_unique<ScopedClipboardHistoryPauseImpl>(
                  weak_ptr->clipboard_history_.get());
            }
            GetClipboard()->WriteClipboardData(std::move(original_data));
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(original_data)),
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
    context_menu_->Cancel();
    return;
  }

  context_menu_->RemoveMenuItemWithCommandId(command_id);
}

void ClipboardHistoryControllerImpl::DeleteClipboardHistoryItem(
    const ClipboardHistoryItem& item) {
  ClipboardHistoryUtil::RecordClipboardHistoryItemDeleted(item);
  clipboard_history_->RemoveItemForId(item.id());
}

void ClipboardHistoryControllerImpl::AdvancePseudoFocus(bool reverse) {
  DCHECK(context_menu_);
  context_menu_->AdvancePseudoFocus(reverse);
}

gfx::Rect ClipboardHistoryControllerImpl::CalculateAnchorRect() const {
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  auto* host = ash::GetWindowTreeHostForDisplay(display.id());

  // Some web apps render the caret in an IFrame, and we will not get the
  // bounds in that case.
  // TODO(https://crbug.com/1099930): Show the menu in the middle of the
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
