// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/wallpaper_search/wallpaper_search_background_manager.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/thread_pool.h"
#include "base/token.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"

namespace {

void WriteFileToPath(const std::string& data, const base::FilePath& path) {
  base::WriteFile(path, base::as_bytes(base::make_span(data)));
}

}  // namespace

// static
void WallpaperSearchBackgroundManager::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kNtpWallpaperSearchHistory,
                             base::Value::List());
}

// static
void WallpaperSearchBackgroundManager::RemoveWallpaperSearchBackground(
    Profile* profile) {
  auto* pref_service = profile->GetPrefs();
  const std::string& local_background_id =
      pref_service->GetString(prefs::kNtpCustomBackgroundLocalToDeviceId);
  if (!local_background_id.empty()) {
    const auto& history =
        pref_service->GetList(prefs::kNtpWallpaperSearchHistory);
    // If it is in history, it should always be the first in history,
    // since an image is moved up in history when it is picked.
    // If it is not in history, we delete its file. Otherwise, we leave
    // the file there for history to use.
    if (history.empty() ||
        (history.front().is_string() &&
         history.front().GetString() != local_background_id)) {
      base::FilePath path = profile->GetPath().AppendASCII(
          local_background_id +
          chrome::kChromeUIUntrustedNewTabPageBackgroundFilename);
      base::ThreadPool::PostTask(
          FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
          base::GetDeleteFileCallback(path));
    }
  }
}

// static
void WallpaperSearchBackgroundManager::ResetProfilePrefs(Profile* profile) {
  auto* pref_service = profile->GetPrefs();
  for (const auto& entry :
       pref_service->GetList(prefs::kNtpWallpaperSearchHistory)) {
    if (entry.is_string()) {
      base::FilePath path = profile->GetPath().AppendASCII(
          entry.GetString() +
          chrome::kChromeUIUntrustedNewTabPageBackgroundFilename);
      base::ThreadPool::PostTask(
          FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
          base::GetDeleteFileCallback(path));
    }
  }
  pref_service->ClearPref(prefs::kNtpWallpaperSearchHistory);
}

WallpaperSearchBackgroundManager::WallpaperSearchBackgroundManager(
    Profile* profile)
    : ntp_custom_background_service_(
          NtpCustomBackgroundServiceFactory::GetForProfile(profile)),
      profile_(profile),
      pref_service_(profile_->GetPrefs()) {
  CHECK(ntp_custom_background_service_);
}

WallpaperSearchBackgroundManager::~WallpaperSearchBackgroundManager() = default;

std::vector<base::Token> WallpaperSearchBackgroundManager::GetHistory() {
  auto& history_list =
      pref_service_->GetList(prefs::kNtpWallpaperSearchHistory);
  std::vector<base::Token> history;
  for (auto& entry : history_list) {
    if (entry.is_string()) {
      auto token = base::Token::FromString(entry.GetString());
      if (token.has_value()) {
        history.push_back(token.value());
      }
    }
  }
  return history;
}

void WallpaperSearchBackgroundManager::SelectHistoryImage(
    const base::Token& id,
    const gfx::Image& image) {
  if (ntp_custom_background_service_->IsCustomBackgroundDisabledByPolicy() ||
      image.IsEmpty()) {
    return;
  }

  ntp_custom_background_service_->SetBackgroundToLocalResourceWithId(id);
  ntp_custom_background_service_->UpdateCustomLocalBackgroundColorAsync(image);
}

void WallpaperSearchBackgroundManager::SelectLocalBackgroundImage(
    const base::Token& id,
    const SkBitmap& bitmap) {
  if (ntp_custom_background_service_->IsCustomBackgroundDisabledByPolicy()) {
    return;
  }

  std::vector<unsigned char> encoded;
  const bool success = gfx::PNGCodec::EncodeBGRASkBitmap(
      bitmap, /*discard_transparency=*/false, &encoded);
  if (success) {
    // Do not update theme image unless it is different from the current.
    // Otherwise, we end up deleting the image file as part of the cleanup
    // of the last theme.
    absl::optional<CustomBackground> current_theme =
        ntp_custom_background_service_->GetCustomBackground();
    if (!current_theme.has_value() ||
        !current_theme->local_background_id.has_value() ||
        current_theme->local_background_id.value() != id) {
      base::ThreadPool::PostTaskAndReply(
          FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
          base::BindOnce(
              &WriteFileToPath, std::string(encoded.begin(), encoded.end()),
              profile_->GetPath().AppendASCII(
                  id.ToString() +
                  chrome::kChromeUIUntrustedNewTabPageBackgroundFilename)),
          base::BindOnce(&WallpaperSearchBackgroundManager::
                             SetBackgroundToLocalResourceWithId,
                         weak_ptr_factory_.GetWeakPtr(), id));
    }

    ntp_custom_background_service_->UpdateCustomLocalBackgroundColorAsync(
        gfx::Image::CreateFrom1xBitmap(bitmap));
  }
}

absl::optional<base::Token>
WallpaperSearchBackgroundManager::SaveCurrentBackgroundToHistory() {
  absl::optional<CustomBackground> current_theme =
      ntp_custom_background_service_->GetCustomBackground();
  if (current_theme.has_value() &&
      current_theme->local_background_id.has_value()) {
    const base::Value::List& current_history =
        pref_service_->GetList(prefs::kNtpWallpaperSearchHistory);
    std::string background_id_str =
        current_theme->local_background_id.value().ToString();
    base::Value::List new_history =
        base::Value::List().Append(background_id_str);
    // Add each value in |current_history| to |new_history| until
    // |new_history| reaches the max size of 6. Do not append the
    // value if it is the same as the id of |current_theme|.
    for (const auto& value : current_history) {
      const std::string* value_str = value.GetIfString();
      if (value_str && *value_str != background_id_str) {
        if (new_history.size() >= 6) {
          // Delete values that will no longer be in the history.
          base::ThreadPool::PostTask(
              FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
              base::GetDeleteFileCallback(profile_->GetPath().AppendASCII(
                  *value_str +
                  chrome::kChromeUIUntrustedNewTabPageBackgroundFilename)));
        } else {
          new_history.Append(*value_str);
        }
      }
    }
    pref_service_->SetList(prefs::kNtpWallpaperSearchHistory,
                           std::move(new_history));
    return current_theme->local_background_id;
  }
  return absl::nullopt;
}

void WallpaperSearchBackgroundManager::SetBackgroundToLocalResourceWithId(
    const base::Token& id) {
  ntp_custom_background_service_->SetBackgroundToLocalResourceWithId(id);
}
