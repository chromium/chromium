// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/magnification_manager.h"

#include <limits>
#include <memory>

#include "ash/magnifier/magnification_controller.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/docked_magnifier_controller.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace chromeos {

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
  prefs->SetBoolean(ash::prefs::kAccessibilityScreenMagnifierEnabled, enabled);
  prefs->CommitPendingWrite();
}

bool MagnificationManager::IsDockedMagnifierEnabled() const {
  return profile_ &&
         profile_->GetPrefs()->GetBoolean(ash::prefs::kDockedMagnifierEnabled);
}

void MagnificationManager::SetDockedMagnifierEnabled(bool enabled) {
  if (!profile_)
    return;

  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(ash::prefs::kDockedMagnifierEnabled, enabled);
  prefs->CommitPendingWrite();
}

void MagnificationManager::SaveScreenMagnifierScale(double scale) {
  if (!profile_)
    return;

  profile_->GetPrefs()->SetDouble(
      ash::prefs::kAccessibilityScreenMagnifierScale, scale);
}

double MagnificationManager::GetSavedScreenMagnifierScale() const {
  if (!profile_)
    return std::numeric_limits<double>::min();

  return profile_->GetPrefs()->GetDouble(
      ash::prefs::kAccessibilityScreenMagnifierScale);
}

void MagnificationManager::OnProfileWillBeDestroyed(Profile* profile) {
  DCHECK_EQ(profile_, profile);
  SetProfile(nullptr);
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
        ash::prefs::kAccessibilityScreenMagnifierEnabled,
        base::BindRepeating(&MagnificationManager::UpdateMagnifierFromPrefs,
                            base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityScreenMagnifierCenterFocus,
        base::BindRepeating(&MagnificationManager::UpdateMagnifierFromPrefs,
                            base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityScreenMagnifierScale,
        base::BindRepeating(&MagnificationManager::UpdateMagnifierFromPrefs,
                            base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kDockedMagnifierEnabled,
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

  ash::Shell::Get()->magnification_controller()->SetEnabled(enabled);
}

void MagnificationManager::SetMagnifierKeepFocusCenteredInternal(
    bool keep_focus_centered) {
  if (keep_focus_centered_ == keep_focus_centered)
    return;

  keep_focus_centered_ = keep_focus_centered;

  ash::Shell::Get()->magnification_controller()->SetKeepFocusCentered(
      keep_focus_centered_);
}

void MagnificationManager::SetMagnifierScaleInternal(double scale) {
  if (scale_ == scale)
    return;

  scale_ = scale;

  ash::Shell::Get()->magnification_controller()->SetScale(scale_,
                                                          false /* animate */);
}

void MagnificationManager::UpdateMagnifierFromPrefs() {
  if (!profile_)
    return;

  PrefService* prefs = profile_->GetPrefs();
  const bool enabled =
      prefs->GetBoolean(ash::prefs::kAccessibilityScreenMagnifierEnabled);
  const bool keep_focus_centered =
      prefs->GetBoolean(ash::prefs::kAccessibilityScreenMagnifierCenterFocus);
  const double scale =
      prefs->GetDouble(ash::prefs::kAccessibilityScreenMagnifierScale);

  if (!enabled) {
    SetMagnifierEnabledInternal(enabled);
    SetMagnifierKeepFocusCenteredInternal(keep_focus_centered);
    SetMagnifierScaleInternal(scale);
  } else {
    SetMagnifierScaleInternal(scale);
    SetMagnifierKeepFocusCenteredInternal(keep_focus_centered);
    SetMagnifierEnabledInternal(enabled);
  }

  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_SCREEN_MAGNIFIER,
                                          fullscreen_magnifier_enabled_);

  if (!AccessibilityManager::Get())
    return;
  AccessibilityManager::Get()->NotifyAccessibilityStatusChanged(details);
  if (ash::Shell::Get())
    ash::Shell::Get()->UpdateCursorCompositingEnabled();
}

void MagnificationManager::UpdateDockedMagnifierFromPrefs() {
  if (!profile_)
    return;

  PrefService* prefs = profile_->GetPrefs();
  const bool enabled = prefs->GetBoolean(ash::prefs::kDockedMagnifierEnabled);
  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_DOCKED_MAGNIFIER,
                                          enabled);

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
  if (bounds_in_screen.IsEmpty())
    return;

  // Fullscreen magnifier and docked magnifier are mutually exclusive.
  if (fullscreen_magnifier_enabled_) {
    ash::Shell::Get()->magnification_controller()->HandleFocusedNodeChanged(
        is_editable, bounds_in_screen);
    return;
  }
  DCHECK(IsDockedMagnifierEnabled());
  ash::DockedMagnifierController::Get()->CenterOnPoint(
      bounds_in_screen.CenterPoint());
}

}  // namespace chromeos
