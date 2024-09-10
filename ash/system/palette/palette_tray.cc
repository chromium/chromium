// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/palette_tray.h"

#include <memory>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/stylus_utils.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/icon_button.h"
#include "ash/style/typography.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/palette/palette_tool_manager.h"
#include "ash/system/palette/palette_utils.h"
#include "ash/system/palette/palette_welcome_bubble.h"
#include "ash/system/palette/stylus_battery_delegate.h"
#include "ash/system/palette/stylus_battery_view.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Padding for tray icon (dp; the button that shows the palette menu).
constexpr int kTrayIconMainAxisInset = 8;
constexpr int kTrayIconCrossAxisInset = 0;

// Width of the palette itself (dp).
constexpr int kPaletteWidth = 332;

// Margins between the title view and the edges around it (dp).
constexpr int kPaddingBetweenTitleAndSeparator = 3;
constexpr int kPaddingBetweenBottomAndLastTrayItem = 8;

// Insets for the title view (dp).
constexpr auto kTitleViewPadding = gfx::Insets::TLBR(8, 16, 8, 16);

// Spacing between buttons in the title view (dp).
constexpr int kTitleViewChildSpacing = 16;

bool HasSomeStylusDisplay() {
  for (const ui::TouchscreenDevice& device :
       ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices()) {
    if (device.has_stylus) {
      return true;
    }
  }
  return false;
}

class TitleView : public views::View {
  METADATA_HEADER(TitleView, views::View)

 public:
  explicit TitleView(PaletteTray* palette_tray) : palette_tray_(palette_tray) {
    // TODO(tdanderson|jdufault): Use TriView to handle the layout of the title.
    // See crbug.com/614453.
    auto box_layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, kTitleViewPadding,
        kTitleViewChildSpacing);
    box_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    views::BoxLayout* layout_ptr = SetLayoutManager(std::move(box_layout));

    auto* title_label = AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_TITLE)));
    title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_label->SetEnabledColorId(kColorAshTextColorPrimary);
    title_label->SetAutoColorReadabilityEnabled(false);
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosTitle1,
                                          *title_label);
    layout_ptr->SetFlexForView(title_label, 1);

    AddChildView(std::make_unique<StylusBatteryView>());

    auto* separator = AddChildView(std::make_unique<views::Separator>());
    separator->SetPreferredLength(GetPreferredSize().height());
    separator->SetColorId(ui::kColorAshSystemUIMenuSeparator);

    help_button_ = AddChildView(std::make_unique<IconButton>(
        base::BindRepeating(
            &TitleView::ButtonPressed, base::Unretained(this),
            PaletteTrayOptions::PALETTE_HELP_BUTTON,
            base::BindRepeating(
                &SystemTrayClient::ShowPaletteHelp,
                base::Unretained(Shell::Get()->system_tray_model()->client()))),
        IconButton::Type::kMedium, &kSystemMenuHelpIcon,
        IDS_ASH_STATUS_TRAY_HELP));
    settings_button_ = AddChildView(std::make_unique<IconButton>(
        base::BindRepeating(
            &TitleView::ButtonPressed, base::Unretained(this),
            PaletteTrayOptions::PALETTE_SETTINGS_BUTTON,
            base::BindRepeating(
                &SystemTrayClient::ShowPaletteSettings,
                base::Unretained(Shell::Get()->system_tray_model()->client()))),
        IconButton::Type::kMedium, &kSystemMenuSettingsIcon,
        IDS_ASH_PALETTE_SETTINGS));
  }

  TitleView(const TitleView&) = delete;
  TitleView& operator=(const TitleView&) = delete;

  ~TitleView() override = default;

 private:
  void ButtonPressed(PaletteTrayOptions option,
                     base::RepeatingClosure callback) {
    std::move(callback).Run();
    palette_tray_->HidePalette();
  }

  // Unowned pointers to button views so we can determine which button was
  // clicked.
  raw_ptr<views::View> settings_button_;
  raw_ptr<views::View> help_button_;
  raw_ptr<PaletteTray, DanglingUntriaged> palette_tray_;
};

BEGIN_METADATA(TitleView)
END_METADATA

// Used as a Shell pre-target handler to notify PaletteTray of stylus events.
class StylusEventHandler : public ui::EventHandler {
 public:
  explicit StylusEventHandler(PaletteTray* tray) : palette_tray_(tray) {
    Shell::Get()->AddPreTargetHandler(this);
  }

  StylusEventHandler(const StylusEventHandler&) = delete;
  StylusEventHandler& operator=(const StylusEventHandler&) = delete;

  ~StylusEventHandler() override { Shell::Get()->RemovePreTargetHandler(this); }

  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override {
    if (event->pointer_details().pointer_type == ui::EventPointerType::kPen) {
      palette_tray_->OnStylusEvent(*event);
    }
  }

 private:
  raw_ptr<PaletteTray> palette_tray_;
};

}  // namespace

PaletteTray::PaletteTray(Shelf* shelf)
    : TrayBackgroundView(shelf, TrayBackgroundViewCatalogName::kPalette),
      palette_tool_manager_(std::make_unique<PaletteToolManager>(this)),
      welcome_bubble_(std::make_unique<PaletteWelcomeBubble>(this)),
      stylus_event_handler_(std::make_unique<StylusEventHandler>(this)),
      scoped_session_observer_(this) {
  SetCallback(base::BindRepeating(&PaletteTray::OnPaletteTrayPressed,
                                  weak_factory_.GetWeakPtr()));

  PaletteTool::RegisterToolInstances(palette_tool_manager_.get());

  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto icon = std::make_unique<views::ImageView>();
  icon->SetTooltipText(l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_TITLE));
  tray_container()->SetMargin(kTrayIconMainAxisInset, kTrayIconCrossAxisInset);
  icon_ = tray_container()->AddChildView(std::move(icon));

  Shell::Get()->AddShellObserver(this);
  Shell::Get()->display_manager()->AddDisplayManagerObserver(this);

  shelf->AddObserver(this);
}

PaletteTray::~PaletteTray() {
  if (bubble_)
    bubble_->bubble_view()->ResetDelegate();

  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
  Shell::Get()->display_manager()->RemoveDisplayManagerObserver(this);
  shelf()->RemoveObserver(this);
}

// static
void PaletteTray::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kHasSeenStylus, false);
}

// static
void PaletteTray::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kEnableStylusTools, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kLaunchPaletteOnEjectEvent, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

bool PaletteTray::ContainsPointInScreen(const gfx::Point& point) {
  if (GetBoundsInScreen().Contains(point))
    return true;

  return bubble_ && bubble_->bubble_view()->GetBoundsInScreen().Contains(point);
}

bool PaletteTray::ShouldShowPalette() const {
  return is_palette_enabled_ && stylus_utils::HasStylusInput() &&
         (HasSomeStylusDisplay() ||
          stylus_utils::IsPaletteEnabledOnEveryDisplay());
}

bool PaletteTray::ShouldShowOnDisplay() {
  if (stylus_utils::IsPaletteEnabledOnEveryDisplay() ||
      display_has_stylus_for_testing_) {
    return true;
  }

  // |widget| is null when this function is called from PaletteTray constructor
  // before it is added to a widget.
  views::Widget* const widget = GetWidget();
  if (!widget)
    return false;

  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          widget->GetNativeWindow());

  // Is there a TouchscreenDevice which targets this display or one of
  // the active mirrors?
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::DisplayIdList ids;
  ids.push_back(display.id());
  if (display_manager->IsInMirrorMode()) {
    display::DisplayIdList mirrors =
        display_manager->GetMirroringDestinationDisplayIdList();
    ids.insert(ids.end(), mirrors.begin(), mirrors.end());
    ids.push_back(display_manager->mirroring_source_id());
  }

  for (const ui::TouchscreenDevice& device :
       ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices()) {
    if (device.has_stylus && base::Contains(ids, device.target_display_id)) {
      return true;
    }
  }

  return false;
}

bool PaletteTray::IsWidgetOnInternalDisplay() {
  // |widget| is null when this function is called from PaletteTray constructor
  // before it is added to a widget.
  views::Widget* const widget = GetWidget();
  if (!widget)
    return false;

  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          widget->GetNativeWindow());

  return display.IsInternal();
}

void PaletteTray::OnStylusEvent(const ui::TouchEvent& event) {
  if (local_state_ && !HasSeenStylus())
    local_state_->SetBoolean(prefs::kHasSeenStylus, true);

  // Flip the enable stylus tools setting if the user has never interacted
  // with it. crbug/1122609
  if (pref_change_registrar_user_ &&
      !pref_change_registrar_user_->prefs()->HasPrefPath(
          prefs::kEnableStylusTools)) {
    pref_change_registrar_user_->prefs()->SetBoolean(prefs::kEnableStylusTools,
                                                     true);
  }

  // Attempt to show the welcome bubble.
  if (!welcome_bubble_->HasBeenShown() && active_user_pref_service_) {
    // If a stylus event is detected on the palette tray, the user already knows
    // about the tray and there is no need to show them the welcome bubble.
    if (!GetBoundsInScreen().Contains(event.target()->GetScreenLocation(event)))
      welcome_bubble_->ShowIfNeeded();
    else
      welcome_bubble_->MarkAsShown();
  }

  if (HasSeenStylus() && welcome_bubble_->HasBeenShown())
    stylus_event_handler_.reset();
}

void PaletteTray::OnActiveUserPrefServiceChanged(PrefService* pref_service) {
  active_user_pref_service_ = pref_service;
  pref_change_registrar_user_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_user_->Init(pref_service);
  pref_change_registrar_user_->Add(
      prefs::kEnableStylusTools,
      base::BindRepeating(&PaletteTray::OnPaletteEnabledPrefChanged,
                          base::Unretained(this)));

  // Read the initial value.
  OnPaletteEnabledPrefChanged();

  // We may need to show the bubble upon switching users for devices with
  // external stylus, but only if the device has seen a stylus before (avoid
  // showing the bubble if the device has never and may never be used with
  // stylus).
  if (HasSeenStylus() && !stylus_utils::HasInternalStylus())
    welcome_bubble_->ShowIfNeeded();
}

void PaletteTray::OnSessionStateChanged(session_manager::SessionState state) {
  UpdateIconVisibility();
  if (HasSeenStylus() && !stylus_utils::HasInternalStylus())
    welcome_bubble_->ShowIfNeeded();
}

void PaletteTray::OnLockStateChanged(bool locked) {
  UpdateIconVisibility();

  if (locked) {
    palette_tool_manager_->DisableActiveTool(PaletteGroup::MODE);

    // The user can eject the stylus during the lock screen transition, which
    // will open the palette. Make sure to close it if that happens.
    HidePalette();
  }
}

void PaletteTray::OnShellInitialized() {
  ProjectorControllerImpl* projector_controller =
      Shell::Get()->projector_controller();
  projector_session_observation_.Observe(
      projector_controller->projector_session());
}

void PaletteTray::OnShellDestroying() {
  projector_session_observation_.Reset();
}

void PaletteTray::OnDidApplyDisplayChanges() {
  UpdateIconVisibility();
}

void PaletteTray::ClickedOutsideBubble(const ui::LocatedEvent& event) {
  HidePalette();
}

void PaletteTray::UpdateTrayItemColor(bool is_active) {
  UpdateTrayIcon();
}

void PaletteTray::OnThemeChanged() {
  TrayBackgroundView::OnThemeChanged();
  UpdateTrayIcon();
}

std::u16string PaletteTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_TITLE);
}

void PaletteTray::HandleLocaleChange() {
  icon_->SetTooltipText(l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_TITLE));
}

void PaletteTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {
  if (bubble_->bubble_view() == bubble_view)
    HidePalette();
}

void PaletteTray::OnInputDeviceConfigurationChanged(
    uint8_t input_device_types) {
  if (input_device_types & ui::InputDeviceEventObserver::kTouchscreen) {
    UpdateIconVisibility();
  }
}

void PaletteTray::OnTouchDeviceAssociationChanged() {
  UpdateIconVisibility();
}

void PaletteTray::OnStylusStateChanged(ui::StylusState stylus_state) {
  // Device may have a stylus but it has been forcibly disabled.
  if (!stylus_utils::HasStylusInput())
    return;

  // Don't do anything if the palette tray is not shown.
  if (!GetVisible())
    return;

  // Only respond on the internal display.
  if (!IsWidgetOnInternalDisplay())
    return;

  // Auto show/hide the palette if allowed by the user.
  if (pref_change_registrar_user_ &&
      pref_change_registrar_user_->prefs()->GetBoolean(
          prefs::kLaunchPaletteOnEjectEvent)) {
    if (stylus_state == ui::StylusState::REMOVED && !bubble_) {
      is_bubble_auto_opened_ = true;
      ShowBubble();
    } else if (stylus_state == ui::StylusState::INSERTED && bubble_) {
      HidePalette();
    }
  } else if (stylus_state == ui::StylusState::REMOVED) {
    // Show the palette welcome bubble if the auto open palette setting is not
    // turned on, if the bubble has not been shown before (|welcome_bubble_|
    // will be nullptr if the bubble has been shown before).
    welcome_bubble_->ShowIfNeeded();
  }

  // Disable any active modes if the stylus has been inserted.
  if (stylus_state == ui::StylusState::INSERTED)
    palette_tool_manager_->DisableActiveTool(PaletteGroup::MODE);
}

void PaletteTray::BubbleViewDestroyed() {
  palette_tool_manager_->NotifyViewsDestroyed();
  // Opening the palette via an accelerator will close any open widget and then
  // open a new one. This method is called when the widget is closed, but due to
  // async close the new bubble may have already been created. If this happens,
  // |bubble_| will not be null.
  SetIsActive(bubble_ || palette_tool_manager_->GetActiveTool(
                             PaletteGroup::MODE) != PaletteToolId::NONE);
}

std::u16string PaletteTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

bool PaletteTray::ShouldEnableExtraKeyboardAccessibility() {
  return Shell::Get()->accessibility_controller()->spoken_feedback().enabled();
}

void PaletteTray::HideBubble(const TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

void PaletteTray::HidePalette() {
  is_bubble_auto_opened_ = false;
  num_actions_in_bubble_ = 0;
  bubble_.reset();

  shelf()->UpdateAutoHideState();
}

void PaletteTray::HidePaletteImmediately() {
  if (bubble_)
    bubble_->bubble_widget()->SetVisibilityChangedAnimationsEnabled(false);
  HidePalette();
}

void PaletteTray::OnProjectorSessionActiveStateChanged(bool active) {
  is_palette_visibility_paused_ = active;
  if (active) {
    DeactivateActiveTool();
    SetVisiblePreferred(false);
  } else {
    UpdateIconVisibility();
  }
}

void PaletteTray::OnActiveToolChanged() {
  ++num_actions_in_bubble_;

  // If there is no tool currently active and the palette tray button was active
  // (eg. a mode was deactivated without pressing the palette tray button), make
  // the palette tray button inactive.
  if (palette_tool_manager_->GetActiveTool(PaletteGroup::MODE) ==
          PaletteToolId::NONE &&
      is_active()) {
    SetIsActive(false);
  }

  UpdateTrayIcon();
}

aura::Window* PaletteTray::GetWindow() {
  return shelf()->GetWindow();
}

void PaletteTray::AnchorUpdated() {
  if (bubble_)
    bubble_->bubble_view()->UpdateBubble();
}

void PaletteTray::Initialize() {
  TrayBackgroundView::Initialize();
  ui::DeviceDataManager::GetInstance()->AddObserver(this);

  InitializeWithLocalState();
}

void PaletteTray::CloseBubbleInternal() {
  HidePalette();
}

void PaletteTray::ShowBubble() {
  if (bubble_)
    return;

  DCHECK(tray_container());

  // There may still be an active tool if show bubble was called from an
  // accelerator.
  DeactivateActiveTool();

  TrayBubbleView::InitParams init_params = CreateInitParamsForTrayBubble(this);
  init_params.preferred_width = kPaletteWidth;

  // TODO(tdanderson): Refactor into common row layout code.
  // TODO(tdanderson|jdufault): Add material design ripple effects to the menu
  // rows.

  // Create and customize bubble view.
  auto bubble_view = std::make_unique<TrayBubbleView>(init_params);
  bubble_view->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(0, 0, kPaddingBetweenBottomAndLastTrayItem, 0)));

  // Add title.
  bubble_view->AddChildView(std::make_unique<TitleView>(this));

  // Add horizontal separator between the title and tools.
  auto* separator =
      bubble_view->AddChildView(std::make_unique<views::Separator>());
  separator->SetColorId(ui::kColorAshSystemUIMenuSeparator);
  separator->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      kPaddingBetweenTitleAndSeparator, 0, kMenuSeparatorVerticalPadding, 0)));

  // Add palette tools.
  // TODO(tdanderson|jdufault): Use SystemMenuButton to get the material design
  // ripples.
  std::vector<PaletteToolView> views = palette_tool_manager_->CreateViews();
  for (const PaletteToolView& view : views) {
    bubble_view->AddChildView(view.view.get());
  }

  // Show the bubble.
  bubble_ = std::make_unique<TrayBubbleWrapper>(this);
  bubble_->ShowBubble(std::move(bubble_view));
  SetIsActive(true);
}

TrayBubbleView* PaletteTray::GetBubbleView() {
  return bubble_ ? bubble_->bubble_view() : nullptr;
}

views::Widget* PaletteTray::GetBubbleWidget() const {
  return bubble_ ? bubble_->GetBubbleWidget() : nullptr;
}

void PaletteTray::InitializeWithLocalState() {
  DCHECK(!local_state_);
  local_state_ = Shell::Get()->local_state();
  // |local_state_| could be null in tests.
  if (!local_state_)
    return;

  // If a device has an internal stylus or the flag to force stylus is set, mark
  // the has seen stylus flag as true since we know the user has a stylus.
  if (stylus_utils::HasInternalStylus() ||
      stylus_utils::HasForcedStylusInput()) {
    local_state_->SetBoolean(prefs::kHasSeenStylus, true);
  }

  pref_change_registrar_local_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_local_->Init(local_state_);
  pref_change_registrar_local_->Add(
      prefs::kHasSeenStylus,
      base::BindRepeating(&PaletteTray::OnHasSeenStylusPrefChanged,
                          base::Unretained(this)));

  OnHasSeenStylusPrefChanged();
}

void PaletteTray::UpdateTrayIcon() {
  SkColor color;
  color = GetColorProvider()->GetColor(
      is_active() ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                  : cros_tokens::kCrosSysOnSurface);
  icon_->SetImage(CreateVectorIcon(
      palette_tool_manager_->GetActiveTrayIcon(
          palette_tool_manager_->GetActiveTool(PaletteGroup::MODE)),
      kTrayIconSize, color));
}

void PaletteTray::OnPaletteEnabledPrefChanged() {
  is_palette_enabled_ = pref_change_registrar_user_->prefs()->GetBoolean(
      prefs::kEnableStylusTools);

  if (!is_palette_enabled_) {
    SetVisiblePreferred(false);
    palette_tool_manager_->DisableActiveTool(PaletteGroup::MODE);
  } else {
    UpdateIconVisibility();
  }
}

void PaletteTray::OnPaletteTrayPressed(const ui::Event& event) {
  if (bubble_) {
    HidePalette();
    return;
  }

  // Do not show the bubble if there was an action on the palette tray while
  // there was an active tool.
  if (DeactivateActiveTool()) {
    SetIsActive(false);
    return;
  }

  ShowBubble();
}

void PaletteTray::OnHasSeenStylusPrefChanged() {
  DCHECK(local_state_);

  UpdateIconVisibility();
}

bool PaletteTray::DeactivateActiveTool() {
  PaletteToolId active_tool_id =
      palette_tool_manager_->GetActiveTool(PaletteGroup::MODE);
  if (active_tool_id != PaletteToolId::NONE) {
    palette_tool_manager_->DeactivateTool(active_tool_id);
    return true;
  }

  return false;
}

bool PaletteTray::HasSeenStylus() {
  return local_state_ && local_state_->GetBoolean(prefs::kHasSeenStylus);
}

void PaletteTray::SetDisplayHasStylusForTesting() {
  display_has_stylus_for_testing_ = true;
  UpdateIconVisibility();
}

void PaletteTray::UpdateIconVisibility() {
  bool visible_preferred =
      is_palette_enabled_ && !is_palette_visibility_paused_ &&
      stylus_utils::HasStylusInput() && ShouldShowOnDisplay() &&
      palette_utils::IsInUserSession();
  SetVisiblePreferred(visible_preferred);
  if (visible_preferred)
    UpdateLayout();
}

void PaletteTray::OnAutoHideStateChanged(ShelfAutoHideState state) {
  if (!bubble_)
    return;

  // The anchor rect should be placed with the `work_area` + the `PaletteTray`'s
  // position on the shelf.
  gfx::Rect work_area = shelf()->GetSystemTrayAnchorRect();
  gfx::Rect tray_anchor = GetBubbleAnchor()->GetAnchorBoundsInScreen();
  gfx::Rect anchor_rect;
  switch (shelf()->alignment()) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      anchor_rect =
          gfx::Rect(base::i18n::IsRTL() ? tray_anchor.x() : tray_anchor.right(),
                    work_area.bottom(), 0, 0);
      break;
    case ShelfAlignment::kLeft:
      anchor_rect = gfx::Rect(work_area.x(), tray_anchor.bottom(), 0, 0);
      break;
    case ShelfAlignment::kRight:
      anchor_rect = gfx::Rect(work_area.right(), tray_anchor.bottom(), 0, 0);
      break;
  }

  bubble_->bubble_view()->ChangeAnchorRect(anchor_rect);
}

BEGIN_METADATA(PaletteTray)
END_METADATA

}  // namespace ash
