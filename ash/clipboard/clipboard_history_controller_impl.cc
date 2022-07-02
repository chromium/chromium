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
#include "base/check_op.h"
#include "base/json/values_util.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/one_shot_event.h"
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

using ClipboardHistoryPasteType =
    ash::ClipboardHistoryControllerImpl::ClipboardHistoryPasteType;
bool IsPlainTextPaste(ClipboardHistoryPasteType paste_type) {
  switch (paste_type) {
    case ClipboardHistoryPasteType::kPlainTextAccelerator:
    case ClipboardHistoryPasteType::kPlainTextKeystroke:
    case ClipboardHistoryPasteType::kPlainTextMouse:
    case ClipboardHistoryPasteType::kPlainTextTouch:
    case ClipboardHistoryPasteType::kPlainTextVirtualKeyboard:
      return true;
    case ClipboardHistoryPasteType::kRichTextAccelerator:
    case ClipboardHistoryPasteType::kRichTextKeystroke:
    case ClipboardHistoryPasteType::kRichTextMouse:
    case ClipboardHistoryPasteType::kRichTextTouch:
    case ClipboardHistoryPasteType::kRichTextVirtualKeyboard:
      return false;
  }
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

bool ClipboardHistoryControllerImpl::IsMenuShowing() const {
  return context_menu_ && context_menu_->IsRunning();
}

void ClipboardHistoryControllerImpl::ToggleMenuShownByAccelerator() {
  if (IsMenuShowing()) {
    // Before hiding the menu, paste the selected menu item, or the first item
    // if none is selected.
    PasteMenuItemData(context_menu_->GetSelectedMenuItemCommand().value_or(
                          ClipboardHistoryUtil::kFirstItemCommandId),
                      ClipboardHistoryPasteType::kRichTextAccelerator);
    return;
  }

  if (ClipboardHistoryUtil::IsEnabledInCurrentMode() && IsEmpty()) {
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

void ClipboardHistoryControllerImpl::BlockGetHistoryValuesForTest() {
  get_history_values_blocker_for_test_.reset();
  get_history_values_blocker_for_test_ = std::make_unique<base::OneShotEvent>();
}

void ClipboardHistoryControllerImpl::ResumeGetHistoryValuesForTest() {
  DCHECK(get_history_values_blocker_for_test_);
  get_history_values_blocker_for_test_->Signal();
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
  // If a test is performing some work that must be done before history values
  // are returned, wait to run this function until that work is finished.
  if (get_history_values_blocker_for_test_ &&
      !get_history_values_blocker_for_test_->is_signaled()) {
    get_history_values_blocker_for_test_->Post(
        FROM_HERE,
        base::BindOnce(
            &ClipboardHistoryControllerImpl::GetHistoryValuesWithEncodedPNGs,
            weak_ptr_factory_.GetWeakPtr(), item_id_filter, std::move(callback),
            std::move(encoded_pngs)));
    return;
  }

  base::Value item_results(base::Value::Type::LIST);
  DCHECK(encoded_pngs);

  // Check after asynchronous PNG encoding finishes to make sure we have not
  // entered a state where clipboard history is disabled, e.g., a locked screen.
  if (!ClipboardHistoryUtil::IsEnabledInCurrentMode()) {
    std::move(callback).Run(std::move(item_results));
    return;
  }

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
              ClipboardHistoryPasteType::kRichTextVirtualKeyboard));
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

    // Verify that this operation did not interleave with a clipboard history
    // paste.
    DCHECK_EQ(pastes_to_be_confirmed_, 0);
    // Whether or not the non-interleaving assumption has held, always reset
    // `pastes_to_be_confirmed_` to prevent standard clipboard pastes from
    // possibly being counted as clipboard history pastes, which could
    // significantly affect the clipboard history consecutive pastes metric.
    pastes_to_be_confirmed_ = 0;
  }

  if (confirmed_operation_callback_for_test_)
    confirmed_operation_callback_for_test_.Run(/*success=*/true);
}

void ClipboardHistoryControllerImpl::OnCachedImageModelUpdated(
    const std::vector<base::UnguessableToken>& menu_item_ids) {
  for (auto& observer : observers_)
    observer.OnClipboardHistoryItemsUpdated(menu_item_ids);
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
      // Create a scope for the variables used in this case so that they can be
      // deallocated from the stack.
      {
        bool paste_plain_text = event_flags & ui::EF_SHIFT_DOWN;
        // There are no specific flags that indicate a paste triggered by a
        // keystroke, so assume by default that keystroke was the event source
        // and then check for the other known possibilities. This assumption may
        // cause pastes from unknown sources to be incorrectly captured as
        // keystroke pastes, but we do not expect such cases to significantly
        // alter metrics.
        ClipboardHistoryPasteType paste_type =
            paste_plain_text ? ClipboardHistoryPasteType::kPlainTextKeystroke
                             : ClipboardHistoryPasteType::kRichTextKeystroke;
        if (event_flags & ui::EF_MOUSE_BUTTON) {
          paste_type = paste_plain_text
                           ? ClipboardHistoryPasteType::kPlainTextMouse
                           : ClipboardHistoryPasteType::kRichTextMouse;
        } else if (event_flags & ui::EF_FROM_TOUCH) {
          paste_type = paste_plain_text
                           ? ClipboardHistoryPasteType::kPlainTextTouch
                           : ClipboardHistoryPasteType::kRichTextTouch;
        }
        PasteMenuItemData(command_id, paste_type);
        return;
      }
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

void ClipboardHistoryControllerImpl::PasteMenuItemData(
    int command_id,
    ClipboardHistoryPasteType paste_type) {
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
                     selected_item, paste_type));
}

void ClipboardHistoryControllerImpl::PasteClipboardHistoryItem(
    aura::Window* intended_window,
    ClipboardHistoryItem item,
    ClipboardHistoryPasteType paste_type) {
  // It's possible that the window could change or we could enter a disabled
  // mode after posting the `PasteClipboardHistoryItem()` task.
  if (!intended_window || intended_window != window_util::GetActiveWindow() ||
      !ClipboardHistoryUtil::IsEnabledInCurrentMode()) {
    if (confirmed_operation_callback_for_test_)
      confirmed_operation_callback_for_test_.Run(/*success=*/false);
    return;
  }

  // Get information about the data to be pasted.
  bool paste_plain_text = IsPlainTextPaste(paste_type);
  auto* clipboard = GetClipboard();
  ui::DataTransferEndpoint data_dst(ui::EndpointType::kClipboardHistory);
  const auto* current_clipboard_data = clipboard->GetClipboardData(&data_dst);
  bool item_not_on_clipboard =
      !current_clipboard_data || *current_clipboard_data != item.data();

  // Clipboard history pastes are performed by temporarily writing data to the
  // system clipboard, if necessary, and then issuing a standard paste.
  // Determine the data we should temporarily write to the clipboard, if any, so
  // that we can paste the selected history item.
  std::unique_ptr<ui::ClipboardData> data_to_paste;
  bool should_restore_clipboard = false;
  if (paste_plain_text) {
    data_to_paste = std::make_unique<ui::ClipboardData>();
    data_to_paste->set_commit_time(item.data().commit_time());
    data_to_paste->set_text(item.data().text());
    ui::DataTransferEndpoint* data_src = item.data().source();
    if (data_src) {
      data_to_paste->set_source(
          std::make_unique<ui::DataTransferEndpoint>(*data_src));
    }
    // When pasting new data that doesn't correspond to any clipboard history
    // item, we always need to restore the clipboard buffer after pasting.
    should_restore_clipboard = true;
  } else if (item_not_on_clipboard) {
    data_to_paste = std::make_unique<ui::ClipboardData>(item.data());
    // If we are reordering clipboard history on paste, then we do not need to
    // restore the clipboard buffer after modifying it for a rich text paste,
    // because the clipboard already reflects the history list's top item.
    should_restore_clipboard = !features::IsClipboardHistoryReorderEnabled();
  }

  // If necessary, replace the clipboard's current data before issuing a paste.
  bool paste_reorders_history =
      item_not_on_clipboard && features::IsClipboardHistoryReorderEnabled();
  std::unique_ptr<ui::ClipboardData> original_data;
  if (data_to_paste) {
    // Pausing clipboard history while manipulating the clipboard prevents the
    // paste item from being added to clipboard history. In cases where we
    // actually want the paste item to end up at the top of history, we
    // accomplish that by not pausing clipboard history entirely; however, we do
    // pause clipboard history metrics. If we did not pause metrics, clipboard
    // history reorders would be erroneously interpreted as copy events.
    ScopedClipboardHistoryPauseImpl scoped_pause(
        clipboard_history_.get(),
        /*metrics_only=*/paste_reorders_history && !paste_plain_text);
    original_data = clipboard->WriteClipboardData(std::move(data_to_paste));
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

  base::UmaHistogramEnumeration("Ash.ClipboardHistory.PasteType", paste_type);

  for (auto& observer : observers_)
    observer.OnClipboardHistoryPasted();

  if (!should_restore_clipboard)
    return;

  // `currently_pasting_` only needs to be set when clipboard history and the
  // clipboard buffer are not in a consistent state for subsequent pastes.
  currently_pasting_ = true;

  // Replace the clipboard data. Some apps take a long time to receive the paste
  // event, and some apps will read from the clipboard multiple times per paste.
  // Wait a bit before writing `data_to_restore` back to the clipboard.
  auto data_to_restore = paste_reorders_history
                             ? std::make_unique<ui::ClipboardData>(item.data())
                             : std::move(original_data);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](const base::WeakPtr<ClipboardHistoryControllerImpl>& weak_ptr,
             std::unique_ptr<ui::ClipboardData> data_to_restore,
             bool paste_reorders_history) {
            std::unique_ptr<ScopedClipboardHistoryPauseImpl> scoped_pause;
            if (weak_ptr) {
              weak_ptr->currently_pasting_ = false;
              // When restoring the original clipboard content, pause clipboard
              // history to avoid committing data already at the top of the
              // clipboard history list. When restoring an item not originally
              // at the top of the clipboard history list, do not pause history
              // entirely, but do pause metrics so that the reorder is not
              // erroneously interpreted as a copy event.
              scoped_pause = std::make_unique<ScopedClipboardHistoryPauseImpl>(
                  weak_ptr->clipboard_history_.get(),
                  /*metrics_only=*/paste_reorders_history);
            }
            GetClipboard()->WriteClipboardData(std::move(data_to_restore));
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(data_to_restore),
          paste_reorders_history),
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
