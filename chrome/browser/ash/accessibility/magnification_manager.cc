// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/magnification_manager.h"

#include <limits>
#include <memory>

#include "ash/accessibility/magnifier/magnification_controller.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/docked_magnifier_controller.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {
namespace {

// The duration of time to ignore focus changes after the last mouse event.
// Keep under one frame length (~16ms at 60hz).
constexpr base::TimeDelta kTimeIgnoreFocusChangeAfterMouseEvent =
    base::TimeDelta::FromMilliseconds(15);

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

void MagnificationManager::HandleFocusedRectChangedIfEnabled(
    const gfx::Rect& bounds_in_screen,
    bool is_editable) {
  if (!fullscreen_magnifier_enabled_ && !IsDockedMagnifierEnabled())
    return;

  HandleFocusChanged(bounds_in_screen, is_editable);
}

void MagnificationManager::HandleMoveMagnifierToRectIfEnabled(
    const gfx::Rect& rect) {
  // Fullscreen magnifier and docked magnifier are mutually exclusive.
  if (fullscreen_magnifier_enabled_) {
    Shell::Get()->magnification_controller()->HandleMoveMagnifierToRect(rect);
    return;
  }
  if (IsDockedMagnifierEnabled()) {
    DockedMagnifierController::Get()->MoveMagnifierToRect(rect);
  }
}

void MagnificationManager::OnMouseEvent(ui::MouseEvent* event) {
  last_mouse_event_ = base::TimeTicks::Now();
}

void MagnificationManager::OnViewEvent(views::View* view,
                                       ax::mojom::Event event_type) {
  if (!fullscreen_magnifier_enabled_ && !IsDockedMagnifierEnabled())
    return;

  if (event_type != ax::mojom::Event::kFocus &&
      event_type != ax::mojom::Event::kSelection) {
    return;
  }

  ui::AXNodeData data;
  view->GetViewAccessibility().GetAccessibleNodeData(&data);

  // Disallow focus on large containers, which probably should not move the
  // magnified viewport to the center of the view.
  if (ui::IsControl(data.role))
    HandleFocusChanged(view->GetBoundsInScreen(), false);
}

void MagnificationManager::SetProfileForTest(Profile* profile) {
  SetProfile(profile);
}

MagnificationManager::MagnificationManager() {
  registrar_.Add(this, chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                 content::NotificationService::AllSources());
  // TODO(warx): observe focus changed in page notification when either
  // fullscreen magnifier or docked magnifier is enabled.
  registrar_.Add(this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
                 content::NotificationService::AllSources());
  user_manager::UserManager::Get()->AddSessionStateObserver(this);
  views::AXEventManager::Get()->AddObserver(this);
}

MagnificationManager::~MagnificationManager() {
  CHECK(this == g_magnification_manager);
  views::AXEventManager::Get()->RemoveObserver(this);
  user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
}

void MagnificationManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE: {
      // Update |profile_| when entering the login screen.
      Profile* profile = ProfileManager::GetActiveUserProfile();
      if (ProfileHelper::IsSigninProfile(profile))
        SetProfile(profile);
      break;
    }
    case content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE: {
      HandleFocusChangedInPage(details);
      break;
    }
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
  SetProfile(ProfileHelper::Get()->GetProfileByUser(user));
}

void MagnificationManager::SetProfile(Profile* profile) {
  if (profile_)
    profile_observer_.Remove(profile_);
  DCHECK(!profile_observer_.IsObservingSources());

  pref_change_registrar_.reset();

  if (profile) {
    // TODO(yoshiki): Move following code to PrefHandler.
    pref_change_registrar_.reset(new PrefChangeRegistrar);
    pref_change_registrar_->Init(profile->GetPrefs());
    pref_change_registrar_->Add(
        prefs::kAccessibilityScreenMagnifierEnabled,
        base::BindRepeating(&MagnificationManager::UpdateMagnifierFromPrefs,
                            base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityScreenMagnifierCenterFocus,
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

    profile_observer_.Add(profile);
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

  Shell::Get()->magnification_controller()->SetEnabled(enabled);
}

void MagnificationManager::SetMagnifierKeepFocusCenteredInternal(
    bool keep_focus_centered) {
  if (keep_focus_centered_ == keep_focus_centered)
    return;

  keep_focus_centered_ = keep_focus_centered;

  Shell::Get()->magnification_controller()->SetKeepFocusCentered(
      keep_focus_centered_);
}

void MagnificationManager::SetMagnifierScaleInternal(double scale) {
  if (scale_ == scale)
    return;

  scale_ = scale;

  Shell::Get()->magnification_controller()->SetScale(scale_,
                                                     false /* animate */);
}

void MagnificationManager::SetMagnifierMouseFollowingModeInternal(
    MagnifierMouseFollowingMode mouse_following_mode) {
  Shell::Get()->magnification_controller()->set_mouse_following_mode(
      mouse_following_mode);
}

void MagnificationManager::UpdateMagnifierFromPrefs() {
  if (!profile_)
    return;

  PrefService* prefs = profile_->GetPrefs();
  const bool enabled =
      prefs->GetBoolean(prefs::kAccessibilityScreenMagnifierEnabled);
  const bool keep_focus_centered =
      prefs->GetBoolean(prefs::kAccessibilityScreenMagnifierCenterFocus);
  const double scale =
      prefs->GetDouble(prefs::kAccessibilityScreenMagnifierScale);
  const MagnifierMouseFollowingMode mouse_following_mode =
      static_cast<MagnifierMouseFollowingMode>(prefs->GetInteger(
          prefs::kAccessibilityScreenMagnifierMouseFollowingMode));

  SetMagnifierMouseFollowingModeInternal(mouse_following_mode);
  SetMagnifierScaleInternal(scale);
  SetMagnifierKeepFocusCenteredInternal(keep_focus_centered);
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

void MagnificationManager::HandleFocusChangedInPage(
    const content::NotificationDetails& details) {
  const bool docked_magnifier_enabled = IsDockedMagnifierEnabled();
  if (!fullscreen_magnifier_enabled_ && !docked_magnifier_enabled)
    return;

  content::FocusedNodeDetails* node_details =
      content::Details<content::FocusedNodeDetails>(details).ptr();
  // Ash uses the InputMethod of the window tree host to observe text input
  // caret bounds changes, which works for both the native UI as well as
  // webpages. We don't need to notify it of editable nodes in this case.
  if (node_details->is_editable_node)
    return;

  HandleFocusChanged(node_details->node_bounds_in_screen,
                     node_details->is_editable_node);
}

void MagnificationManager::HandleFocusChanged(const gfx::Rect& bounds_in_screen,
                                              bool is_editable) {
  if (features::IsMagnifierNewFocusFollowingEnabled())
    return;

  if (bounds_in_screen.IsEmpty())
    return;

  // Ignore focus changes while mouse activity is occurring.
  if (base::TimeTicks::Now() - last_mouse_event_ <
      kTimeIgnoreFocusChangeAfterMouseEvent) {
    return;
  }

  // Fullscreen magnifier and docked magnifier are mutually exclusive.
  if (fullscreen_magnifier_enabled_) {
    Shell::Get()->magnification_controller()->HandleFocusedNodeChanged(
        is_editable, bounds_in_screen);
    return;
  }
  DCHECK(IsDockedMagnifierEnabled());
  DockedMagnifierController::Get()->CenterOnPoint(
      bounds_in_screen.CenterPoint());
}

}  // namespace ash
