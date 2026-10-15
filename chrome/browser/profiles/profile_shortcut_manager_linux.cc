// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_shortcut_manager_linux.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/nix/xdg_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_view_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSamplingOptions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace {

// Size of the avatar image requested from the profile attributes. The browser
// icon is 256x256 and the avatar badge covers half of it.
constexpr int kAvatarSize = 128;

bool g_disabled_for_unit_tests = false;

// Packaging formats that sandbox the browser can't install desktop entries
// for the host, and may require desktop entry IDs to have a package-specific
// prefix.
bool IsRunningInSandboxedPackage(base::Environment& env) {
  return env.HasVar("FLATPAK_ID") || env.HasVar("SNAP") ||
         env.HasVar("CHROME_WEB_APP_DESKTOP_ID_PREFIX");
}

base::FilePath GetDesktopEntryPath(const std::string& desktop_id) {
  base::Environment env;
  return profiles::internal::GetProfileDesktopEntryDirectory(&env).Append(
      desktop_id + ".desktop");
}

// Deletes the profile icons in `profile_path`, except for `keep`.
void DeleteProfileIcons(const base::FilePath& profile_path,
                        const base::FilePath& keep) {
  base::FileEnumerator enumerator(
      profile_path, /*recursive=*/false, base::FileEnumerator::FILES,
      base::StrCat(
          {profiles::internal::kProfileDesktopEntryIconPrefix, "-*.png"}));
  for (base::FilePath icon = enumerator.Next(); !icon.empty();
       icon = enumerator.Next()) {
    if (icon != keep) {
      base::DeleteFile(icon);
    }
  }
}

// Writes the desktop entry and its icon. If `update_existing_only` is true,
// nothing is written unless the desktop entry already exists. Returns the path
// of the icon on success.
std::optional<base::FilePath> WriteDesktopEntryAndIcon(
    const base::FilePath& chrome_exe,
    const base::FilePath& profile_path,
    const std::string& desktop_id,
    const std::u16string& name,
    const SkBitmap& icon,
    bool update_existing_only) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // The profile directory may have been deleted in the meantime.
  if (!base::DirectoryExists(profile_path)) {
    return std::nullopt;
  }
  const base::FilePath desktop_entry_path = GetDesktopEntryPath(desktop_id);
  if (update_existing_only && !base::PathExists(desktop_entry_path)) {
    // The desktop entry was deleted externally, so its icon is orphaned.
    DeleteProfileIcons(profile_path, /*keep=*/base::FilePath());
    return std::nullopt;
  }

  std::optional<std::vector<uint8_t>> png =
      gfx::PNGCodec::EncodeBGRASkBitmap(icon, /*discard_transparency=*/false);
  if (!png) {
    return std::nullopt;
  }
  const base::FilePath icon_path = profile_path.Append(base::StringPrintf(
      "%s-%08x.png", profiles::internal::kProfileDesktopEntryIconPrefix,
      base::PersistentHash(*png)));
  if (!base::PathExists(icon_path) &&
      !base::ImportantFileWriter::WriteFileAtomically(
          icon_path, base::as_string_view(*png))) {
    return std::nullopt;
  }

  if (!base::CreateDirectory(desktop_entry_path.DirName()) ||
      !base::ImportantFileWriter::WriteFileAtomically(
          desktop_entry_path,
          shell_integration_linux::GetDesktopFileContentsForProfile(
              chrome_exe, profile_path, desktop_id, name, icon_path))) {
    return std::nullopt;
  }
  DeleteProfileIcons(profile_path, /*keep=*/icon_path);
  return icon_path;
}

void DeleteDesktopEntryAndIcon(const base::FilePath& profile_path,
                               const std::string& desktop_id) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::DeleteFile(GetDesktopEntryPath(desktop_id));
  DeleteProfileIcons(profile_path, /*keep=*/base::FilePath());
}

bool DesktopEntryExists(const std::string& desktop_id) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  return base::PathExists(GetDesktopEntryPath(desktop_id));
}

}  // namespace

namespace profiles::internal {

base::FilePath GetProfileDesktopEntryDirectory(base::Environment* env) {
  return base::nix::GetXDGDataWriteLocation(env).Append("applications");
}

SkBitmap BadgeIconWithAvatar(const SkBitmap& app_icon, const SkBitmap& avatar) {
  if (app_icon.drawsNothing() || avatar.drawsNothing()) {
    return app_icon;
  }

  const int width = app_icon.width();
  const int height = app_icon.height();
  const float badge_size = std::min(width, height) / 2.0f;
  const SkRect badge_rect = SkRect::MakeXYWH(
      width - badge_size, height - badge_size, badge_size, badge_size);

  SkBitmap badged;
  badged.allocN32Pixels(width, height);
  SkCanvas canvas(badged, SkSurfaceProps{});
  canvas.clear(SK_ColorTRANSPARENT);
  canvas.drawImage(app_icon.asImage(), 0, 0);

  // Cut a transparent ring around the badge so that it stands out from the
  // browser icon.
  SkPaint clear_paint;
  clear_paint.setAntiAlias(true);
  clear_paint.setBlendMode(SkBlendMode::kClear);
  const float ring_width = badge_size / 16.0f;
  canvas.drawOval(badge_rect.makeOutset(ring_width, ring_width), clear_paint);

  // Draw the center square of the avatar, clipped to a circle.
  const float avatar_side = std::min(avatar.width(), avatar.height());
  const SkRect avatar_rect = SkRect::MakeXYWH(
      (avatar.width() - avatar_side) / 2.0f,
      (avatar.height() - avatar_side) / 2.0f, avatar_side, avatar_side);
  canvas.save();
  canvas.clipRRect(SkRRect::MakeOval(badge_rect), /*doAntiAlias=*/true);
  canvas.drawImageRect(
      avatar.asImage(), avatar_rect, badge_rect,
      SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kLinear),
      /*paint=*/nullptr, SkCanvas::kStrict_SrcRectConstraint);
  canvas.restore();
  return badged;
}

}  // namespace profiles::internal

void ProfileShortcutManager::DisableForUnitTests() {
  g_disabled_for_unit_tests = true;
}

// static
bool ProfileShortcutManager::IsFeatureEnabled() {
  if (g_disabled_for_unit_tests ||
      !base::FeatureList::IsEnabled(features::kLinuxPerProfileAppId)) {
    return false;
  }
  base::Environment env;
  if (IsRunningInSandboxedPackage(env)) {
    return false;
  }
  // Per-profile desktop entry IDs only exist for profiles in the default user
  // data directory.
  return chrome::IsUsingDefaultDataDirectory().value_or(false);
}

// static
std::unique_ptr<ProfileShortcutManager> ProfileShortcutManager::Create(
    ProfileManager* manager) {
  return std::make_unique<ProfileShortcutManagerLinux>(manager);
}

ProfileShortcutManagerLinux::ProfileShortcutManagerLinux(
    ProfileManager* manager)
    : profile_manager_(manager),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {
  storage_observation_.Observe(
      &profile_manager_->GetProfileAttributesStorage());
}

ProfileShortcutManagerLinux::~ProfileShortcutManagerLinux() = default;

void ProfileShortcutManagerLinux::CreateOrUpdateProfileIcon(
    const base::FilePath& profile_path) {
  // The icon is part of the desktop entry, so there's nothing to do for
  // profiles without one.
  CreateOrUpdateDesktopEntry(profile_path, Mode::kUpdateExistingOnly);
}

void ProfileShortcutManagerLinux::CreateProfileShortcut(
    const base::FilePath& profile_path) {
  CreateOrUpdateDesktopEntry(profile_path, Mode::kCreate);
}

void ProfileShortcutManagerLinux::RemoveProfileShortcuts(
    const base::FilePath& profile_path) {
  if (ProfileAttributesEntry* entry =
          profile_manager_->GetProfileAttributesStorage()
              .GetProfileAttributesWithPath(profile_path)) {
    entry->SetShortcutName(std::u16string());
  }
  RemoveDesktopEntry(profile_path);
}

void ProfileShortcutManagerLinux::HasProfileShortcuts(
    const base::FilePath& profile_path,
    base::OnceCallback<void(bool)> callback) {
  const std::string desktop_id =
      shell_integration_linux::GetPerProfileDesktopId(profile_path);
  if (desktop_id.empty()) {
    std::move(callback).Run(false);
    return;
  }
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&DesktopEntryExists, desktop_id),
      std::move(callback));
}

void ProfileShortcutManagerLinux::GetShortcutProperties(
    const base::FilePath& profile_path,
    base::CommandLine* command_line,
    std::wstring* name,
    base::FilePath* icon_path) {
  *command_line =
      base::CommandLine(shell_integration_linux::internal::GetChromeExePath());
  command_line->AppendSwitchPath(switches::kProfileDirectory,
                                 profile_path.BaseName());
  if (const ProfileAttributesEntry* entry =
          profile_manager_->GetProfileAttributesStorage()
              .GetProfileAttributesWithPath(profile_path)) {
    *name = base::UTF16ToWide(entry->GetName());
  }
  auto it = icon_paths_.find(profile_path);
  *icon_path = it != icon_paths_.end() ? it->second : base::FilePath();
}

bool ProfileShortcutManagerLinux::CanCreateProfileShortcut(
    const base::FilePath& profile_path) {
  return !shell_integration_linux::GetPerProfileDesktopId(profile_path).empty();
}

void ProfileShortcutManagerLinux::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const std::u16string& profile_name) {
  RemoveDesktopEntry(profile_path);
  generations_.erase(profile_path);

  // Like on Windows, profile shortcuts only exist while there are multiple
  // profiles. The settings page hides the shortcut toggle for the last
  // profile, so remove its desktop entry rather than leaving it stranded.
  ProfileAttributesStorage& storage =
      profile_manager_->GetProfileAttributesStorage();
  if (storage.GetNumberOfProfiles() == 1u) {
    const ProfileAttributesEntry* last_profile =
        storage.GetAllProfilesAttributes().front();
    if (!last_profile->GetShortcutName().empty()) {
      RemoveProfileShortcuts(last_profile->GetPath());
    }
  }
}

void ProfileShortcutManagerLinux::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const std::u16string& old_profile_name) {
  CreateOrUpdateDesktopEntry(profile_path, Mode::kUpdateExistingOnly);
}

void ProfileShortcutManagerLinux::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  CreateOrUpdateDesktopEntry(profile_path, Mode::kUpdateExistingOnly);
}

void ProfileShortcutManagerLinux::OnProfileHighResAvatarLoaded(
    const base::FilePath& profile_path) {
  CreateOrUpdateDesktopEntry(profile_path, Mode::kUpdateExistingOnly);
}

void ProfileShortcutManagerLinux::FlushForTesting(base::OnceClosure callback) {
  task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                 std::move(callback));
}

void ProfileShortcutManagerLinux::CreateOrUpdateDesktopEntry(
    const base::FilePath& profile_path,
    Mode mode) {
  ProfileAttributesEntry* entry =
      profile_manager_->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_path);
  if (!entry) {
    return;
  }
  // The shortcut name is set while the profile has a desktop entry, or one is
  // being written.
  if (mode == Mode::kUpdateExistingOnly && entry->GetShortcutName().empty()) {
    return;
  }
  const std::string desktop_id =
      shell_integration_linux::GetPerProfileDesktopId(profile_path);
  if (desktop_id.empty()) {
    return;
  }

  const std::u16string name =
      base::StrCat({l10n_util::GetStringUTF16(IDS_PRODUCT_NAME), u" (",
                    entry->GetName(), u")"});
  SkBitmap app_icon;
  if (const gfx::ImageSkia* image =
          ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_PRODUCT_LOGO_256)) {
    app_icon = *image->bitmap();
  }
  const SkBitmap avatar =
      entry
          ->GetAvatarIcon(kAvatarSize, /*use_high_res_file=*/true,
                          /*icon_params=*/{.has_padding = false})
          .AsBitmap();
  SkBitmap icon = profiles::internal::BadgeIconWithAvatar(app_icon, avatar);
  // SkBitmaps share pixel storage, so pass a deep copy to the other sequence.
  SkBitmap icon_copy;
  if (!icon_copy.tryAllocPixels(icon.info()) ||
      !icon.readPixels(icon_copy.pixmap())) {
    return;
  }

  // Set the shortcut name right away, so that updates while the entry is
  // being created aren't dropped. It is cleared again if the write fails.
  entry->SetShortcutName(entry->GetName());
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &WriteDesktopEntryAndIcon,
          shell_integration_linux::internal::GetChromeExePath(), profile_path,
          desktop_id, name, std::move(icon_copy),
          /*update_existing_only=*/mode == Mode::kUpdateExistingOnly),
      base::BindOnce(&ProfileShortcutManagerLinux::OnDesktopEntryWritten,
                     weak_ptr_factory_.GetWeakPtr(), profile_path,
                     StartOperation(profile_path)));
}

void ProfileShortcutManagerLinux::RemoveDesktopEntry(
    const base::FilePath& profile_path) {
  StartOperation(profile_path);
  icon_paths_.erase(profile_path);
  shell_integration_linux::SetPerProfileDesktopEntryInstalled(profile_path,
                                                              false);
  const std::string desktop_id =
      shell_integration_linux::GetPerProfileDesktopId(profile_path);
  if (desktop_id.empty()) {
    return;
  }
  task_runner_->PostTask(FROM_HERE, base::BindOnce(&DeleteDesktopEntryAndIcon,
                                                   profile_path, desktop_id));
}

void ProfileShortcutManagerLinux::OnDesktopEntryWritten(
    const base::FilePath& profile_path,
    uint64_t generation,
    std::optional<base::FilePath> icon_path) {
  // A later write or removal for this profile has been started since; its
  // reply decides the profile's state.
  auto it = generations_.find(profile_path);
  if (it == generations_.end() || it->second != generation) {
    return;
  }
  ProfileAttributesEntry* entry =
      profile_manager_->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_path);
  if (!entry) {
    return;
  }
  if (!icon_path) {
    // The entry couldn't be written, or the user deleted it. Forget about it,
    // so that later profile changes don't recreate it.
    entry->SetShortcutName(std::u16string());
    icon_paths_.erase(profile_path);
    shell_integration_linux::SetPerProfileDesktopEntryInstalled(profile_path,
                                                                false);
    return;
  }
  icon_paths_[profile_path] = *icon_path;
  if (!shell_integration_linux::GetInstalledPerProfileDesktopId(profile_path)
           .empty()) {
    // The entry was only updated.
    return;
  }
  // Give the desktop environment time to pick up the new entry before
  // windows start referring to it.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ProfileShortcutManagerLinux::OnDesktopEntryRegistered,
                     weak_ptr_factory_.GetWeakPtr(), profile_path, generation),
      kDesktopEntryRegistrationDelay);
}

void ProfileShortcutManagerLinux::OnDesktopEntryRegistered(
    const base::FilePath& profile_path,
    uint64_t generation) {
  auto it = generations_.find(profile_path);
  if (it == generations_.end() || it->second != generation) {
    return;
  }
  // The profile's open browser windows switch to the per-profile identity, and
  // windows opened from now on use it.
  shell_integration_linux::SetPerProfileDesktopEntryInstalled(profile_path,
                                                              true);
}

uint64_t ProfileShortcutManagerLinux::StartOperation(
    const base::FilePath& profile_path) {
  return ++generations_[profile_path];
}
