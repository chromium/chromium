// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/ime/arc_ime_service.h"

#include <tuple>
#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/ime/arc_ime_bridge_impl.h"
#include "ash/components/arc/ime/arc_ime_util.h"
#include "ash/components/arc/ime/key_event_result_receiver.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_content_view.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/constants.h"
#include "ui/base/ime/ime_key_event_dispatcher.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/range/range.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"
#include "ui/wm/core/ime_util_chromeos.h"

namespace arc {

namespace {

std::optional<double> g_override_default_device_scale_factor;
std::optional<gfx::Point> g_override_display_origin;

// Return true when a rich text editing is available on a text field with the
// given type.
bool IsTextInputActive(ui::TextInputType type) {
  return type != ui::TEXT_INPUT_TYPE_NONE && type != ui::TEXT_INPUT_TYPE_NULL;
}

// Return true if the given key event generats a visible character.
bool IsCharacterKeyEvent(const ui::KeyEvent* event) {
  return !IsControlChar(event) && !ui::IsSystemKeyModifier(event->flags());
}

// Return true if the given key event is used for language switching by IME.
// Please refer to `ash::InputMethodAsh::DispatchKeyEvent` for details.
bool IsLanguageInputKey(const ui::KeyEvent* event) {
  switch (event->key_code()) {
    case ui::VKEY_CONVERT:
    case ui::VKEY_NONCONVERT:
    case ui::VKEY_DBE_SBCSCHAR:
    case ui::VKEY_DBE_DBCSCHAR:
      return true;
    default:
      return false;
  }
}

int CursorBehaviorToCursorPosition(
    ui::TextInputClient::InsertTextCursorBehavior cursor_behavior) {
  switch (cursor_behavior) {
    case ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText:
      return 1;
    case ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorBeforeText:
      return 0;
  }
}

class ArcWindowDelegateImpl : public ArcImeService::ArcWindowDelegate {
 public:
  explicit ArcWindowDelegateImpl(ArcImeService* ime_service)
      : ime_service_(ime_service) {}

  ArcWindowDelegateImpl(const ArcWindowDelegateImpl&) = delete;
  ArcWindowDelegateImpl& operator=(const ArcWindowDelegateImpl&) = delete;

  ~ArcWindowDelegateImpl() override = default;

  bool IsInArcAppWindow(const aura::Window* window) const override {
    // WMHelper is not created in browser_tests.
    if (!exo::WMHelper::HasInstance())
      return false;
    for (; window; window = window->parent()) {
      if (ash::IsArcWindow(window))
        return true;

      // TODO(crbug.com/1168334): Find a correct way to detect the ARC++
      // notifications. It should be okay for now because only the ARC++ windows
      // have kSkipImeProcessing.
      if (window->GetProperty(aura::client::kSkipImeProcessing))
        return true;
    }
    return false;
  }

  void RegisterFocusObserver() override {
    // WMHelper is not craeted in browser_tests.
    if (!exo::WMHelper::HasInstance())
      return;
    exo::WMHelper::GetInstance()->AddFocusObserver(ime_service_);
  }

  void UnregisterFocusObserver() override {
    // If WMHelper is already destroyed, do nothing.
    // TODO(crbug.com/40531599): Fix shutdown order.
    if (!exo::WMHelper::HasInstance())
      return;
    exo::WMHelper::GetInstance()->RemoveFocusObserver(ime_service_);
  }

  ui::InputMethod* GetInputMethodForWindow(
      aura::Window* window) const override {
    if (!window || !window->GetHost())
      return nullptr;
    return window->GetHost()->GetInputMethod();
  }

 private:
  const raw_ptr<ArcImeService> ime_service_;
};

// Singleton factory for ArcImeService.
class ArcImeServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcImeService,
          ArcImeServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcImeServiceFactory";

  static ArcImeServiceFactory* GetInstance() {
    return base::Singleton<ArcImeServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcImeServiceFactory>;
  ArcImeServiceFactory() = default;
  ~ArcImeServiceFactory() override = default;
};

}  // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
// ArcImeService main implementation:

// static
ArcImeService* ArcImeService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcImeServiceFactory::GetForBrowserContext(context);
}

ArcImeService::ArcImeService(content::BrowserContext* context,
                             ArcBridgeService* bridge_service)
    : ArcImeService(context,
                    bridge_service,
                    std::make_unique<ArcWindowDelegateImpl>(this)) {}

ArcImeService::ArcImeService(content::BrowserContext* context,
                             ArcBridgeService* bridge_service,
                             std::unique_ptr<ArcWindowDelegate> delegate)
    : ime_bridge_(new ArcImeBridgeImpl(this, bridge_service)),
      arc_window_delegate_(std::move(delegate)),
      ime_type_(ui::TEXT_INPUT_TYPE_NONE),
      ime_flags_(ui::TEXT_INPUT_FLAG_NONE),
      is_personalized_learning_allowed_(false),
      has_composition_text_(false),
      receiver_(std::make_unique<KeyEventResultReceiver>()) {
  if (aura::Env::HasInstance())
    aura::Env::GetInstance()->AddObserver(this);
  arc_window_delegate_->RegisterFocusObserver();
}

ArcImeService::~ArcImeService() {
  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method)
    input_method->DetachTextInputClient(this);

  if (focused_arc_window_)
    focused_arc_window_->RemoveObserver(this);
  arc_window_delegate_->UnregisterFocusObserver();
  if (aura::Env::HasInstance())
    aura::Env::GetInstance()->RemoveObserver(this);

  // KeyboardController is destroyed before ArcImeService (except in tests),
  // so check whether there is a KeyboardController first before removing |this|
  // from KeyboardController observers.
  if (keyboard::KeyboardUIController::HasInstance()) {
    auto* keyboard_controller = keyboard::KeyboardUIController::Get();
    if (keyboard_controller->HasObserver(this))
      keyboard_controller->RemoveObserver(this);
  }
}

void ArcImeService::SetImeBridgeForTesting(
    std::unique_ptr<ArcImeBridge> test_ime_bridge) {
  ime_bridge_ = std::move(test_ime_bridge);
}

ui::InputMethod* ArcImeService::GetInputMethod() {
  return arc_window_delegate_->GetInputMethodForWindow(focused_arc_window_);
}

void ArcImeService::ReattachInputMethod(aura::Window* old_window,
                                        aura::Window* new_window) {
  ui::InputMethod* const old_ime =
      arc_window_delegate_->GetInputMethodForWindow(old_window);
  ui::InputMethod* const new_ime =
      arc_window_delegate_->GetInputMethodForWindow(new_window);

  if (old_ime != new_ime) {
    if (old_ime)
      old_ime->DetachTextInputClient(this);
    if (new_ime)
      new_ime->SetFocusedTextInputClient(this);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from aura::EnvObserver:

void ArcImeService::OnWindowInitialized(aura::Window* new_window) {
  if (keyboard::KeyboardUIController::HasInstance()) {
    auto* keyboard_controller = keyboard::KeyboardUIController::Get();
    if (keyboard_controller->IsEnabled() &&
        !keyboard_controller->HasObserver(this)) {
      keyboard_controller->AddObserver(this);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from aura::WindowObserver:

void ArcImeService::OnWindowDestroying(aura::Window* window) {
  // This shouldn't be reached on production, since the window lost the focus
  // and called OnWindowFocused() before destroying.
  // But we handle this case for testing.
  if (window == focused_arc_window_)
    OnWindowFocused(nullptr, focused_arc_window_);
}

void ArcImeService::OnWindowRemovingFromRootWindow(aura::Window* window,
                                                   aura::Window* new_root) {
  // IMEs are associated with root windows, hence we may need to detach/attach.
  if (window == focused_arc_window_)
    ReattachInputMethod(focused_arc_window_, new_root);
}

void ArcImeService::OnWindowRemoved(aura::Window* removed_window) {
  // |this| can lose the IME focus because |focused_arc_window_| may have
  // children other than ExoSurface e.g. WebContentsViewAura for CustomTabs.
  // Restore the IME focus when such a window is removed.
  ReattachInputMethod(nullptr, focused_arc_window_);
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from exo::WMHelper::FocusChangeObserver:

void ArcImeService::OnWindowFocused(aura::Window* gained_focus,
                                    aura::Window* lost_focus) {
  if (lost_focus == gained_focus)
    return;

  const bool detach = (lost_focus && focused_arc_window_ == lost_focus);
  const bool attach = arc_window_delegate_->IsInArcAppWindow(gained_focus);

  if (detach) {
    // The focused window and the toplevel window are different in production,
    // but in tests they can be the same, so avoid adding the observer twice.
    if (focused_arc_window_ != focused_arc_window_->GetToplevelWindow())
      focused_arc_window_->GetToplevelWindow()->RemoveObserver(this);
    focused_arc_window_->RemoveObserver(this);
    focused_arc_window_ = nullptr;
  }
  if (attach) {
    DCHECK_EQ(nullptr, focused_arc_window_);
    focused_arc_window_ = gained_focus;
    focused_arc_window_->AddObserver(this);
    // The focused window and the toplevel window are different in production,
    // but in tests they can be the same, so avoid adding the observer twice.
    if (focused_arc_window_ != focused_arc_window_->GetToplevelWindow())
      focused_arc_window_->GetToplevelWindow()->AddObserver(this);
  }

  ReattachInputMethod(detach ? lost_focus : nullptr, focused_arc_window_);
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from arc::ArcImeBridge::Delegate

void ArcImeService::OnTextInputTypeChanged(
    ui::TextInputType type,
    bool is_personalized_learning_allowed,
    int flags) {
  if (ime_type_ == type &&
      is_personalized_learning_allowed_ == is_personalized_learning_allowed &&
      ime_flags_ == flags) {
    return;
  }
  ime_type_ = type;
  is_personalized_learning_allowed_ = is_personalized_learning_allowed;
  ime_flags_ = flags;

  if (!ShouldSendUpdateToInputMethod())
    return;

  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method)
    input_method->OnTextInputTypeChanged(this);

  // Call HideKeyboard() here. On a text field on an ARC++ app, just having
  // non-null text input type doesn't mean the virtual keyboard is necessary. If
  // the virtual keyboard is really needed, ShowVirtualKeyboardIfEnabled will be
  // called later.
  if (keyboard::KeyboardUIController::HasInstance()) {
    auto* keyboard_controller = keyboard::KeyboardUIController::Get();
    if (keyboard_controller->IsEnabled())
      keyboard_controller->HideKeyboardImplicitlyBySystem();
  }
}

void ArcImeService::OnCursorRectChanged(
    const gfx::Rect& rect,
    mojom::CursorCoordinateSpace coordinate_space) {
  if (!ShouldSendUpdateToInputMethod())
    return;

  InvalidateSurroundingTextAndSelectionRange();
  if (!UpdateCursorRect(rect, coordinate_space))
    return;

  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method)
    input_method->OnCaretBoundsChanged(this);
}

void ArcImeService::OnCancelComposition() {
  if (!ShouldSendUpdateToInputMethod())
    return;

  InvalidateSurroundingTextAndSelectionRange();
  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method)
    input_method->CancelComposition(this);
}

void ArcImeService::ShowVirtualKeyboardIfEnabled() {
  if (!ShouldSendUpdateToInputMethod())
    return;

  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method && input_method->GetTextInputClient() == this) {
    input_method->SetVirtualKeyboardVisibilityIfEnabled(true);
  }
}

void ArcImeService::OnCursorRectChangedWithSurroundingText(
    const gfx::Rect& rect,
    const gfx::Range& text_range,
    const std::u16string& text_in_range,
    const gfx::Range& selection_range,
    mojom::CursorCoordinateSpace coordinate_space) {
  if (!ShouldSendUpdateToInputMethod())
    return;

  if (!UpdateCursorRect(rect, coordinate_space) && text_range_ == text_range &&
      text_in_range_ == text_in_range && selection_range_ == selection_range) {
    return;
  }

  text_range_ = text_range;
  text_in_range_ = text_in_range;
  selection_range_ = selection_range;

  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method)
    input_method->OnCaretBoundsChanged(this);
}

void ArcImeService::SendKeyEvent(std::unique_ptr<ui::KeyEvent> key_event,
                                 KeyEventDoneCallback callback) {
  ui::InputMethod* const input_method = GetInputMethod();
  receiver_->SetCallback(std::move(callback), key_event.get());

  if (input_method)
    std::ignore = input_method->DispatchKeyEvent(key_event.get());
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from ash::KeyboardControllerObserver
void ArcImeService::OnKeyboardAppearanceChanged(
    const ash::KeyboardStateDescriptor& state) {
  if (state.is_temporary) {
    return;
  }

  gfx::Rect new_bounds = state.occluded_bounds_in_screen;
  // Multiply by the scale factor. To convert from Chrome DIP to Android pixels.
  gfx::Rect bounds_in_px =
      gfx::ScaleToEnclosingRect(new_bounds, GetDeviceScaleFactorForKeyboard());

  ime_bridge_->SendOnKeyboardAppearanceChanging(bounds_in_px, state.is_visible);
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from ui::TextInputClient:

base::WeakPtr<ui::TextInputClient> ArcImeService::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ArcImeService::SetCompositionText(const ui::CompositionText& composition) {
  InvalidateSurroundingTextAndSelectionRange();
  has_composition_text_ = !composition.text.empty();
  ime_bridge_->SendSetCompositionText(composition);
}

size_t ArcImeService::ConfirmCompositionText(bool keep_selection) {
  if (!keep_selection) {
    InvalidateSurroundingTextAndSelectionRange();
  }
  has_composition_text_ = false;
  // Note: SendConfirmCompositonText() will commit the text and
  // keep the selection unchanged
  ime_bridge_->SendConfirmCompositionText();
  return std::numeric_limits<size_t>::max();
}

void ArcImeService::ClearCompositionText() {
  InvalidateSurroundingTextAndSelectionRange();
  if (has_composition_text_) {
    has_composition_text_ = false;
    ime_bridge_->SendInsertText(std::u16string(), /*new_cursor_position=*/1);
  }
}

void ArcImeService::InsertText(const std::u16string& text,
                               InsertTextCursorBehavior cursor_behavior) {
  InvalidateSurroundingTextAndSelectionRange();
  has_composition_text_ = false;
  ime_bridge_->SendInsertText(text,
                              CursorBehaviorToCursorPosition(cursor_behavior));
}

void ArcImeService::InsertChar(const ui::KeyEvent& event) {
  // According to the document in text_input_client.h, InsertChar() is called
  // even when the text editing is not available. We ignore such events, since
  // for ARC we are only interested in the event as a method of text input.
  if (!IsTextInputActive(ime_type_))
    return;

  InvalidateSurroundingTextAndSelectionRange();

  if (IsCharacterKeyEvent(&event)) {
    has_composition_text_ = false;
    ime_bridge_->SendInsertText(std::u16string(1, event.GetCharacter()),
                                /*new_cursor_position=*/1);
  }
}

ui::TextInputType ArcImeService::GetTextInputType() const {
  return ime_type_;
}

gfx::Rect ArcImeService::GetCaretBounds() const {
  return cursor_rect_;
}

gfx::Rect ArcImeService::GetSelectionBoundingBox() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Rect();
}

bool ArcImeService::GetTextRange(gfx::Range* range) const {
  if (!text_range_.IsValid())
    return false;
  *range = text_range_;
  return true;
}

bool ArcImeService::GetEditableSelectionRange(gfx::Range* range) const {
  if (!selection_range_.IsValid())
    return false;
  *range = selection_range_;
  return true;
}

bool ArcImeService::GetTextFromRange(const gfx::Range& range,
                                     std::u16string* text) const {
  // It's supposed that this method is called only from
  // InputMethod::OnCaretBoundsChanged(). In that method, the range obtained
  // from GetTextRange() is used as the argument of this method. To prevent an
  // unexpected usage, the check, |range != text_range_|, is added.
  if (!text_range_.IsValid() || range != text_range_)
    return false;
  *text = text_in_range_;
  return true;
}

void ArcImeService::EnsureCaretNotInRect(const gfx::Rect& rect_in_screen) {
  if (focused_arc_window_ == nullptr)
    return;
  aura::Window* top_level_window = focused_arc_window_->GetToplevelWindow();
  // If the window is not a notification, the window move is handled by
  // Android.
  if (top_level_window->GetType() != aura::client::WINDOW_TYPE_POPUP)
    return;
  wm::EnsureWindowNotInRect(top_level_window, rect_in_screen);
}

ui::TextInputMode ArcImeService::GetTextInputMode() const {
  return ui::TEXT_INPUT_MODE_DEFAULT;
}

base::i18n::TextDirection ArcImeService::GetTextDirection() const {
  return base::i18n::UNKNOWN_DIRECTION;
}

void ArcImeService::ExtendSelectionAndDelete(size_t before, size_t after) {
  InvalidateSurroundingTextAndSelectionRange();
  ime_bridge_->SendExtendSelectionAndDelete(before, after);
}

int ArcImeService::GetTextInputFlags() const {
  return ime_flags_;
}

bool ArcImeService::CanComposeInline() const {
  return true;
}

bool ArcImeService::GetCompositionCharacterBounds(size_t index,
                                                  gfx::Rect* rect) const {
  return false;
}

bool ArcImeService::HasCompositionText() const {
  return has_composition_text_;
}

ui::TextInputClient::FocusReason ArcImeService::GetFocusReason() const {
  // TODO(https://crbug.com/824604): Determine how the current input client got
  // focused.
  NOTIMPLEMENTED_LOG_ONCE();
  return ui::TextInputClient::FOCUS_REASON_OTHER;
}

bool ArcImeService::GetCompositionTextRange(gfx::Range* range) const {
  return false;
}

bool ArcImeService::SetEditableSelectionRange(const gfx::Range& range) {
  selection_range_ = range;
  ime_bridge_->SendSelectionRange(selection_range_);
  return true;
}

bool ArcImeService::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction) {
  return false;
}

bool ArcImeService::IsTextEditCommandEnabled(
    ui::TextEditCommand command) const {
  return false;
}

ukm::SourceId ArcImeService::GetClientSourceForMetrics() const {
  // TODO(yhanada): Implement this method. crbug.com/752657
  NOTIMPLEMENTED_LOG_ONCE();
  return ukm::SourceId();
}

bool ArcImeService::ShouldDoLearning() {
  return is_personalized_learning_allowed_;
}

bool ArcImeService::SetCompositionFromExistingText(
    const gfx::Range& range,
    const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) {
  if (text_range_.IsValid() && !range.IsBoundedBy(text_range_))
    return false;

  InvalidateSurroundingTextAndSelectionRange();
  has_composition_text_ = !range.is_empty();

  // The sent |range| might be already invalid if the textfield state in Android
  // side is changed simultaneously. It's okay because InputConnection's
  // setComposingRegion handles invalid region correctly.
  ime_bridge_->SendSetComposingRegion(range);
  return true;
}

gfx::Range ArcImeService::GetAutocorrectRange() const {
  // TODO(crbug.com/40134032): Implement this method.
  return gfx::Range();
}

gfx::Rect ArcImeService::GetAutocorrectCharacterBounds() const {
  // TODO(crbug.com/40623107): Implement this method.
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Rect();
}

bool ArcImeService::SetAutocorrectRange(const gfx::Range& range) {
  if (!range.is_empty()) {
    base::UmaHistogramEnumeration("InputMethod.Assistive.Autocorrect.Count",
                                  TextInputClient::SubClass::kArcImeService);
  }
  // TODO(crbug.com/40134032): Implement this method.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

std::optional<ui::GrammarFragment> ArcImeService::GetGrammarFragmentAtCursor()
    const {
  // TODO(crbug.com/40178699): Implement this method.
  NOTIMPLEMENTED_LOG_ONCE();
  return std::nullopt;
}

bool ArcImeService::ClearGrammarFragments(const gfx::Range& range) {
  // TODO(crbug.com/40178699): Implement this method.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool ArcImeService::AddGrammarFragments(
    const std::vector<ui::GrammarFragment>& fragments) {
  if (!fragments.empty()) {
    base::UmaHistogramEnumeration("InputMethod.Assistive.Grammar.Count",
                                  TextInputClient::SubClass::kArcImeService);
  }
  // TODO(crbug.com/40178699): Implement this method.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void ArcImeService::OnDispatchingKeyEventPostIME(ui::KeyEvent* event) {
  if (receiver_->HasCallback() && receiver_->DispatchKeyEventPostIME(event)) {
    event->SetHandled();
    return;
  }

  // Do not forward the key event from virtual keyboard if it's sent via
  // InsertChar(). By the special logic in
  // `ash::InputMethodAsh::DispatchKeyEvent`, both of InsertChar() and
  // DispatchKeyEventPostIME() are called for a key event injected by the
  // virtual keyboard. The below logic stops key event propagation through
  // DispatchKeyEventPostIME() to prevent from inputting two characters.
  const bool from_vk =
      event->properties() && (event->properties()->find(ui::kPropertyFromVK) !=
                              event->properties()->end());
  if (from_vk && IsCharacterKeyEvent(event) && IsTextInputActive(ime_type_))
    event->SetHandled();

  // Do not forward the language input key event from virtual keyboard because
  // it's already handled by `ash::InputMethodAsh`.
  if (from_vk && IsLanguageInputKey(event))
    event->SetHandled();

  // Do no forward a fabricated key event which is not originated from a
  // physical key event. Such a key event is a signal from IME to show they are
  // going to insert/delete text. ARC apps should not see any key event caused
  // by it.
  if (event->key_code() == ui::VKEY_PROCESSKEY && IsTextInputActive(ime_type_))
    event->SetHandled();
}

// static
void ArcImeService::SetOverrideDefaultDeviceScaleFactorForTesting(
    std::optional<double> scale_factor) {
  g_override_default_device_scale_factor = scale_factor;
}

// static
void ArcImeService::SetOverrideDisplayOriginForTesting(
    std::optional<gfx::Point> origin) {
  g_override_display_origin = origin;
}

void ArcImeService::InvalidateSurroundingTextAndSelectionRange() {
  text_range_ = gfx::Range::InvalidRange();
  text_in_range_ = std::u16string();
  selection_range_ = gfx::Range::InvalidRange();
}

bool ArcImeService::UpdateCursorRect(
    const gfx::Rect& rect,
    mojom::CursorCoordinateSpace coordinate_space) {
  gfx::Rect converted;
  if (coordinate_space == mojom::CursorCoordinateSpace::NOTIFICATION) {
    if (!focused_arc_window_)
      return false;

    // Rect is always scaled by the default device scale factor for
    // notification windows.
    converted =
        gfx::ScaleToEnclosingRect(rect, 1.0 / GetDefaultDeviceScaleFactor());

    // Convert the rect from a "notification display" coordinate into the window
    // coordinate. Because notification are aligned in horizontally on the
    // Android side, we just divide x coordinate by the width of the
    // notification window.
    converted.set_x(
        converted.x() %
        ash::ArcNotificationContentView::GetNotificationContentViewWidth());

    // Convert the window coordinate into the screen coordinate.
    converted.Offset(
        focused_arc_window_->GetBoundsInScreen().OffsetFromOrigin());
  } else if (focused_arc_window_) {
    // Convert from Android pixels to Chrome DIP.
    converted = gfx::ScaleToEnclosingRect(
        rect, 1.0 / GetDeviceScaleFactorForFocusedWindow());

    if (coordinate_space == mojom::CursorCoordinateSpace::DISPLAY) {
      // Convert into the screen coordinate.
      const gfx::Point display_origin = GetDisplayOriginForFocusedWindow();
      converted.Offset(display_origin.x(), display_origin.y());
    }

    auto* window = focused_arc_window_->GetToplevelWindow();
    auto* widget = views::Widget::GetWidgetForNativeWindow(window);
    // Check fullscreen window as well because it's possible for ARC to request
    // frame regardless of window state.
    bool covers_display =
        widget && (widget->IsMaximized() || widget->IsFullscreen());
    if (covers_display) {
      auto* frame_view = widget->non_client_view()->frame_view();
      // The frame height will be subtracted from client bounds.
      gfx::Rect bounds =
          frame_view->GetWindowBoundsForClientBounds(gfx::Rect());
      converted.Offset(0, -bounds.y());
    }
  }

  if (cursor_rect_ == converted)
    return false;
  cursor_rect_ = converted;
  return true;
}

bool ArcImeService::ShouldSendUpdateToInputMethod() const {
  // New text input state received from Android should not be sent to
  // InputMethod when the focus is on a non-ARC window. Text input state updates
  // can be sent from Android anytime because there is a dummy input view in
  // Android which is synchronized with the text input on a non-ARC window.
  return focused_arc_window_ != nullptr;
}

double ArcImeService::GetDeviceScaleFactorForKeyboard() const {
  if (g_override_default_device_scale_factor.has_value())
    return g_override_default_device_scale_factor.value();
  if (!exo::WMHelper::HasInstance() ||
      !keyboard::KeyboardUIController::HasInstance()) {
    return 1.0;
  }
  aura::Window* const keyboard_window =
      keyboard::KeyboardUIController::Get()->GetKeyboardWindow();
  if (!keyboard_window)
    return 1.0;
  return exo::WMHelper::GetInstance()->GetDeviceScaleFactorForWindow(
      keyboard_window);
}

double ArcImeService::GetDeviceScaleFactorForFocusedWindow() const {
  DCHECK(focused_arc_window_);
  if (g_override_default_device_scale_factor.has_value())
    return g_override_default_device_scale_factor.value();
  if (!exo::WMHelper::HasInstance())
    return 1.0;
  return exo::WMHelper::GetInstance()->GetDeviceScaleFactorForWindow(
      focused_arc_window_);
}

double ArcImeService::GetDefaultDeviceScaleFactor() const {
  if (g_override_default_device_scale_factor.has_value())
    return g_override_default_device_scale_factor.value();
  if (!exo::WMHelper::HasInstance())
    return 1.0;
  return exo::GetDefaultDeviceScaleFactor();
}

gfx::Point ArcImeService::GetDisplayOriginForFocusedWindow() const {
  DCHECK(focused_arc_window_);
  if (g_override_display_origin.has_value())
    return g_override_display_origin.value();
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(focused_arc_window_)
      .bounds()
      .origin();
}

// static
void ArcImeService::EnsureFactoryBuilt() {
  ArcImeServiceFactory::GetInstance();
}

}  // namespace arc
