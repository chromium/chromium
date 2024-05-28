// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/magnification_manager.h"

#include <limits>
#include <memory>

#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/focused_node_details.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {
namespace {

MagnificationManager* g_magnification_manager = nullptr;

}  // namespace

// static
void MagnificationManager::Initialize() {
  CHECK(g_magnification_manager == nullptr);
  g_magnification_manager = new MagnificationManager();
}

// static
void MagnificationManager::Shutdown() {
  CHECK(g_magnification_manager);
  delete g_magnification_manager;
  g_magnification_manager = nullptr;
}

// static
MagnificationManager* MagnificationManager::Get() {
  return g_magnification_manager;
}

bool MagnificationManager::IsMagnifierEnabled() const {
  return fullscreen_magnifier_enabled_;
}

void MagnificationManager::SetMagnifierEnabled(bool enabled) {
  if (!profile_)
    return;

  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled, enabled);
  prefs->CommitPendingWrite();
}

bool MagnificationManager::IsDockedMagnifierEnabled() const {
  return profile_ &&
         profile_->GetPrefs()->GetBoolean(prefs::kDockedMagnifierEnabled);
}

void MagnificationManager::SetDockedMagnifierEnabled(bool enabled) {
  if (!profile_)
    return;

  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(prefs::kDockedMagnifierEnabled, enabled);
  prefs->CommitPendingWrite();
}

void MagnificationManager::SaveScreenMagnifierScale(double scale) {
  if (!profile_)
    return;

  profile_->GetPrefs()->SetDouble(prefs::kAccessibilityScreenMagnifierScale,
                                  scale);
}

double MagnificationManager::GetSavedScreenMagnifierScale() const {
  if (!profile_)
    return std::numeric_limits<double>::min();

  return profile_->GetPrefs()->GetDouble(
      prefs::kAccessibilityScreenMagnifierScale);
}

void MagnificationManager::OnProfileWillBeDestroyed(Profile* profile) {
  DCHECK_EQ(profile_, profile);
  SetProfile(nullptr);
}

void MagnificationManager::HandleMoveMagnifierToRectIfEnabled(
    const gfx::Rect& rect) {
  // Fullscreen magnifier and docked magnifier are mutually exclusive.
  if (fullscreen_magnifier_enabled_) {
    Shell::Get()->fullscreen_magnifier_controller()->HandleMoveMagnifierToRect(
        rect);
    return;
  }
  if (IsDockedMagnifierEnabled())
    Shell::Get()->docked_magnifier_controller()->MoveMagnifierToRect(rect);
}

void MagnificationManager::HandleMagnifierCenterOnPointIfEnabled(
    const gfx::Point& point_in_screen) {
  // Fullscreen magnifier and docked magnifier are mutually exclusive.
  if (fullscreen_magnifier_enabled_) {
    Shell::Get()->fullscreen_magnifier_controller()->CenterOnPoint(
        point_in_screen);
    return;
  }
  if (IsDockedMagnifierEnabled()) {
    Shell::Get()->docked_magnifier_controller()->CenterOnPoint(point_in_screen);
  }
}

void MagnificationManager::OnMouseEvent(ui::MouseEvent* event) {
  last_mouse_event_ = base::TimeTicks::Now();
}

void MagnificationManager::OnViewEvent(views::View* view,
                                       ax::mojom::Event event_type) {
  if (!view) {
    return;
  }

  if (!fullscreen_magnifier_enabled_ && !IsDockedMagnifierEnabled()) {
    return;
  }

  if (event_type != ax::mojom::Event::kFocus &&
      event_type != ax::mojom::Event::kSelection) {
    return;
  }

  ui::AXNodeData data;
  view->GetViewAccessibility().GetAccessibleNodeData(&data);
}

void MagnificationManager::SetProfileForTest(Profile* profile) {
  SetProfile(profile);
}

MagnificationManager::MagnificationManager() {
  session_observation_.Observe(session_manager::SessionManager::Get());
  user_manager::UserManager::Get()->AddSessionStateObserver(this);
  views::AXEventManager::Get()->AddObserver(this);
}

MagnificationManager::~MagnificationManager() {
  CHECK(this == g_magnification_manager);
  auto* event_manager = views::AXEventManager::Get();
  if (event_manager) {
    event_manager->RemoveObserver(this);
  }
  auto* user_manager = user_manager::UserManager::Get();
  if (user_manager) {
    user_manager->RemoveSessionStateObserver(this);
  }
}

void MagnificationManager::OnLoginOrLockScreenVisible() {
  // Update `profile_` when entering the login screen.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (IsSigninBrowserContext(profile)) {
    SetProfile(profile);
  }
}

void MagnificationManager::ActiveUserChanged(user_manager::User* active_user) {
  if (!active_user)
    return;

  active_user->AddProfileCreatedObserver(
      base::BindOnce(&MagnificationManager::SetProfileByUser,
                     weak_ptr_factory_.GetWeakPtr(), active_user));
}

void MagnificationManager::SetProfileByUser(const user_manager::User* user) {
  SetProfile(Profile::FromBrowserContext(
      BrowserContextHelper::Get()->GetBrowserContextByUser(user)));
}

void MagnificationManager::SetProfile(Profile* profile) {
  if (profile_) {
    DCHECK(profile_observation_.IsObservingSource(profile_.get()));
    profile_observation_.Reset();
  }
  DCHECK(!profile_observation_.IsObserving());

  pref_change_registrar_.reset();

  if (profile) {
    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(profile->GetPrefs());
    pref_change_registrar_->Add(
        prefs::kAccessibilityScreenMagnifierEnabled,
        base::BindRepeating(&MagnificationManager::UpdateMagnifierFromPrefs,
                            base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityScreenMagnifierScale,
        base::BindRepeating(&MagnificationManager::UpdateMagnifierFromPrefs,
                            base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityScreenMagnifierMouseFollowingMode,
        base::BindRepeating(&MagnificationManager::UpdateMagnifierFromPrefs,
                            base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kDockedMagnifierEnabled,
        base::BindRepeating(
            &MagnificationManager::UpdateDockedMagnifierFromPrefs,
            base::Unretained(this)));

    profile_observation_.Observe(profile);
  }

  profile_ = profile;
  UpdateMagnifierFromPrefs();
  UpdateDockedMagnifierFromPrefs();
}

void MagnificationManager::SetMagnifierEnabledInternal(bool enabled) {
  // This method may be invoked even when the other magnifier settings (e.g.
  // type or scale) are changed, so we need to call magnification controller
  // even if |enabled| is unchanged. Only if |enabled| is false and the
  // magnifier is already disabled, we are sure that we don't need to reflect
  // the new settings right now because the magnifier keeps disabled.
  if (!enabled && !fullscreen_magnifier_enabled_)
    return;

  fullscreen_magnifier_enabled_ = enabled;

  Shell::Get()->fullscreen_magnifier_controller()->SetEnabled(enabled);
}

void MagnificationManager::SetMagnifierScaleInternal(double scale) {
  if (scale_ == scale)
    return;

  scale_ = scale;

  Shell::Get()->fullscreen_magnifier_controller()->SetScale(
      scale_, false /* animate */);
}

void MagnificationManager::SetMagnifierMouseFollowingModeInternal(
    MagnifierMouseFollowingMode mouse_following_mode) {
  Shell::Get()->fullscreen_magnifier_controller()->set_mouse_following_mode(
      mouse_following_mode);
}

void MagnificationManager::UpdateMagnifierFromPrefs() {
  if (!profile_)
    return;

  PrefService* prefs = profile_->GetPrefs();
  const bool enabled =
      prefs->GetBoolean(prefs::kAccessibilityScreenMagnifierEnabled);
  const double scale =
      prefs->GetDouble(prefs::kAccessibilityScreenMagnifierScale);
  const MagnifierMouseFollowingMode mouse_following_mode =
      static_cast<MagnifierMouseFollowingMode>(prefs->GetInteger(
          prefs::kAccessibilityScreenMagnifierMouseFollowingMode));

  SetMagnifierMouseFollowingModeInternal(mouse_following_mode);
  SetMagnifierScaleInternal(scale);
  SetMagnifierEnabledInternal(enabled);

  AccessibilityStatusEventDetails details(
      AccessibilityNotificationType::kToggleScreenMagnifier,
      fullscreen_magnifier_enabled_);

  if (!AccessibilityManager::Get())
    return;
  AccessibilityManager::Get()->NotifyAccessibilityStatusChanged(details);
  if (Shell::Get())
    Shell::Get()->UpdateCursorCompositingEnabled();
}

void MagnificationManager::UpdateDockedMagnifierFromPrefs() {
  if (!profile_)
    return;

  PrefService* prefs = profile_->GetPrefs();
  const bool enabled = prefs->GetBoolean(prefs::kDockedMagnifierEnabled);
  AccessibilityStatusEventDetails details(
      AccessibilityNotificationType::kToggleDockedMagnifier, enabled);

  if (!AccessibilityManager::Get())
    return;
  AccessibilityManager::Get()->NotifyAccessibilityStatusChanged(details);
}

}  // namespace ash
