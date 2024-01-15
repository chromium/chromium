// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/dlp_ash.h"

#include "ash/shell.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/screen_manager_ash.h"
#include "chrome/browser/ash/crosapi/window_util.h"
#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash_utils.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"

namespace crosapi {

namespace {

policy::DlpRulesManager::Level ConvertMojoToDlpRulesManagerLevel(
    crosapi::mojom::DlpRestrictionLevel level) {
  switch (level) {
    case crosapi::mojom::DlpRestrictionLevel::kReport:
      return policy::DlpRulesManager::Level::kReport;
    case crosapi::mojom::DlpRestrictionLevel::kWarn:
      return policy::DlpRulesManager::Level::kWarn;
    case crosapi::mojom::DlpRestrictionLevel::kBlock:
      return policy::DlpRulesManager::Level::kBlock;
    case crosapi::mojom::DlpRestrictionLevel::kAllow:
      return policy::DlpRulesManager::Level::kAllow;
    case crosapi::mojom::DlpRestrictionLevel::kNotSet:
      return policy::DlpRulesManager::Level::kNotSet;
  }
}

policy::DlpContentRestrictionSet ConvertMojoToDlpContentRestrictionSet(
    const mojom::DlpRestrictionSetPtr& restrictions) {
  policy::DlpContentRestrictionSet result;
  result.SetRestriction(
      policy::DlpContentRestriction::kScreenshot,
      ConvertMojoToDlpRulesManagerLevel(restrictions->screenshot->level),
      restrictions->screenshot->url);
  result.SetRestriction(
      policy::DlpContentRestriction::kPrivacyScreen,
      ConvertMojoToDlpRulesManagerLevel(restrictions->privacy_screen->level),
      restrictions->privacy_screen->url);
  result.SetRestriction(
      policy::DlpContentRestriction::kPrint,
      ConvertMojoToDlpRulesManagerLevel(restrictions->print->level),
      restrictions->print->url);
  result.SetRestriction(
      policy::DlpContentRestriction::kScreenShare,
      ConvertMojoToDlpRulesManagerLevel(restrictions->screen_share->level),
      restrictions->screen_share->url);
  return result;
}

policy::dlp::FileAction ConvertMojoToDlpFileAction(mojom::FileAction action) {
  switch (action) {
    case crosapi::mojom::FileAction::kUnknown:
      return policy::dlp::FileAction::kUnknown;
    case crosapi::mojom::FileAction::kDownload:
      return policy::dlp::FileAction::kDownload;
    case crosapi::mojom::FileAction::kTransfer:
      return policy::dlp::FileAction::kTransfer;
    case crosapi::mojom::FileAction::kUpload:
      return policy::dlp::FileAction::kUpload;
    case crosapi::mojom::FileAction::kCopy:
      return policy::dlp::FileAction::kCopy;
    case crosapi::mojom::FileAction::kMove:
      return policy::dlp::FileAction::kMove;
    case crosapi::mojom::FileAction::kOpen:
      return policy::dlp::FileAction::kOpen;
    case crosapi::mojom::FileAction::kShare:
      return policy::dlp::FileAction::kShare;
  }
}

content::DesktopMediaID AreaToDesktopMediaID(
    const mojom::ScreenShareAreaPtr& area) {
  // Fullscreen share.
  if (!area->window_id.has_value() && area->snapshot_source_id == 0) {
    return content::DesktopMediaID::RegisterNativeWindow(
        content::DesktopMediaID::TYPE_SCREEN,
        ash::Shell::GetPrimaryRootWindow());
  }

  aura::Window* window = nullptr;
  if (area->window_id.has_value()) {
    window = GetShellSurfaceWindow(area->window_id.value());
  } else if (area->snapshot_source_id != 0) {
    window = crosapi::CrosapiManager::Get()
                 ->crosapi_ash()
                 ->screen_manager_ash()
                 ->GetWindowById(area->snapshot_source_id);
  }

  if (!window)
    return content::DesktopMediaID();

  return content::DesktopMediaID::RegisterNativeWindow(
      content::DesktopMediaID::TYPE_WINDOW, window);
}

}  // namespace

DlpAsh::DlpAsh() {
  receivers_.set_disconnect_handler(base::BindRepeating(
      &DlpAsh::OnDisconnect, weak_ptr_factory_.GetWeakPtr()));
}

DlpAsh::~DlpAsh() = default;

void DlpAsh::BindReceiver(mojo::PendingReceiver<mojom::Dlp> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void DlpAsh::DlpRestrictionsUpdated(const std::string& window_id,
                                    mojom::DlpRestrictionSetPtr restrictions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  policy::DlpContentManagerAsh* dlp_content_manager =
      policy::DlpContentManagerAsh::Get();
  DCHECK(dlp_content_manager);
  dlp_content_manager->OnWindowRestrictionChanged(
      receivers_.current_receiver(), window_id,
      ConvertMojoToDlpContentRestrictionSet(restrictions));
}

void DlpAsh::CheckScreenShareRestriction(
    mojom::ScreenShareAreaPtr area,
    const std::u16string& application_title,
    CheckScreenShareRestrictionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const content::DesktopMediaID media_id = AreaToDesktopMediaID(area);
  if (media_id.is_null()) {
    std::move(callback).Run(/*allowed=*/true);
    return;
  }

  policy::DlpContentManagerAsh* dlp_content_manager =
      policy::DlpContentManagerAsh::Get();
  DCHECK(dlp_content_manager);
  dlp_content_manager->CheckScreenShareRestriction(media_id, application_title,
                                                   std::move(callback));
}

void DlpAsh::OnScreenShareStarted(
    const std::string& label,
    mojom::ScreenShareAreaPtr area,
    const ::std::u16string& application_title,
    ::mojo::PendingRemote<mojom::StateChangeDelegate> delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const content::DesktopMediaID media_id = AreaToDesktopMediaID(area);
  if (media_id.is_null())
    return;

  mojo::RemoteSetElementId id =
      screen_share_remote_delegates_.Add(std::move(delegate));
  base::RepeatingCallback stop_callback = base::BindRepeating(
      &DlpAsh::StopScreenShare, weak_ptr_factory_.GetWeakPtr(), id);
  base::RepeatingCallback state_change_callback = base::BindRepeating(
      &DlpAsh::ChangeScreenShareState, weak_ptr_factory_.GetWeakPtr(), id);

  policy::DlpContentManagerAsh* dlp_content_manager =
      policy::DlpContentManagerAsh::Get();
  DCHECK(dlp_content_manager);
  // Source change callback should not be called for screen or window shares
  // that are controlled over crosapi.
  dlp_content_manager->OnScreenShareStarted(
      label, {media_id}, application_title, std::move(stop_callback),
      std::move(state_change_callback), /*source_callback=*/base::DoNothing());
}

void DlpAsh::OnScreenShareStopped(const std::string& label,
                                  mojom::ScreenShareAreaPtr area) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const content::DesktopMediaID media_id = AreaToDesktopMediaID(area);
  if (media_id.is_null())
    return;

  policy::DlpContentManagerAsh* dlp_content_manager =
      policy::DlpContentManagerAsh::Get();
  DCHECK(dlp_content_manager);
  dlp_content_manager->OnScreenShareStopped(label, media_id);
}

void DlpAsh::ShowBlockedFiles(std::optional<uint64_t> task_id,
                              const std::vector<base::FilePath>& blocked_files,
                              mojom::FileAction action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  DCHECK(profile);
  ::policy::files_controller_ash_utils::ShowDlpBlockedFiles(
      profile, task_id, blocked_files, ConvertMojoToDlpFileAction(action));
}

void DlpAsh::ChangeScreenShareState(
    mojo::RemoteSetElementId id,
    const content::DesktopMediaID& media_id,
    blink::mojom::MediaStreamStateChange new_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!screen_share_remote_delegates_.Contains(id))
    return;
  switch (new_state) {
    case blink::mojom::MediaStreamStateChange::PAUSE:
      screen_share_remote_delegates_.Get(id)->OnPause();
      break;
    case blink::mojom::MediaStreamStateChange::PLAY:
      screen_share_remote_delegates_.Get(id)->OnResume();
      break;
  }
}

void DlpAsh::StopScreenShare(mojo::RemoteSetElementId id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!screen_share_remote_delegates_.Contains(id))
    return;
  screen_share_remote_delegates_.Get(id)->OnStop();
}

void DlpAsh::OnDisconnect() {
  policy::DlpContentManagerAsh* dlp_content_manager =
      policy::DlpContentManagerAsh::Get();
  if (!dlp_content_manager) {
    return;
  }
  dlp_content_manager->CleanPendingRestrictions(receivers_.current_receiver());
}

}  // namespace crosapi
