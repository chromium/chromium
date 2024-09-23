// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/wallpaper_search/wallpaper_search_background_manager.h"

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "base/token.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/search/background/wallpaper_search/wallpaper_search_data.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"

namespace {

const char kWallpaperSearchHistoryId[] = "id";
const char kWallpaperSearchHistoryMood[] = "mood";
const char kWallpaperSearchHistoryStyle[] = "style";
const char kWallpaperSearchHistorySubject[] = "subject";

void WriteFileToPath(const std::string& data, const base::FilePath& path) {
  base::WriteFile(path, base::as_bytes(base::make_span(data)));
}

void DeleteWallpaperSearchImage(const std::string& id,
                                const base::FilePath& profile_path) {
  base::FilePath path = profile_path.AppendASCII(
      id + chrome::kChromeUIUntrustedNewTabPageBackgroundFilename);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::GetDeleteFileCallback(path));
}

std::optional<HistoryEntry> GetHistoryEntryFromPrefValue(
    const base::Value& pref_value) {
  if (pref_value.is_dict()) {
    const base::Value::Dict& pref_dict = pref_value.GetDict();
    const std::string* id_string =
        pref_dict.FindString(kWallpaperSearchHistoryId);
    if (id_string) {
      std::optional<base::Token> id = base::Token::FromString(*id_string);
      if (id.has_value()) {
        HistoryEntry history_entry = HistoryEntry(*id);
        const std::string* subject_string =
            pref_dict.FindString(kWallpaperSearchHistorySubject);
        if (subject_string) {
          history_entry.subject = *subject_string;
        }
        const std::string* style_string =
            pref_dict.FindString(kWallpaperSearchHistoryStyle);
        if (style_string) {
          history_entry.style = *style_string;
        }
        const std::string* mood_string =
            pref_dict.FindString(kWallpaperSearchHistoryMood);
        if (mood_string) {
          history_entry.mood = *mood_string;
        }
        return history_entry;
      }
    }
  }
  return std::nullopt;
}

base::Value::Dict GetHistoryEntryDict(const HistoryEntry& history_entry) {
  base::Value::Dict history_entry_dict = base::Value::Dict().Set(
      kWallpaperSearchHistoryId, history_entry.id.ToString());
  if (history_entry.subject) {
    history_entry_dict.Set(kWallpaperSearchHistorySubject,
                           *history_entry.subject);
  }
  if (history_entry.style) {
    history_entry_dict.Set(kWallpaperSearchHistoryStyle, *history_entry.style);
  }
  if (history_entry.mood) {
    history_entry_dict.Set(kWallpaperSearchHistoryMood, *history_entry.mood);
  }
  return history_entry_dict;
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
    // If it is not in history, we delete its file. Otherwise, we leave
    // the file there for history to use.
    bool found = false;
    for (const base::Value& entry : history) {
      const auto entry_obj = GetHistoryEntryFromPrefValue(entry);
      if (entry_obj.has_value() &&
          entry_obj->id.ToString() == local_background_id) {
        found = true;
        break;
      }
    }
    if (!found) {
      DeleteWallpaperSearchImage(local_background_id, profile->GetPath());
    }
  }
}

// static
void WallpaperSearchBackgroundManager::ResetProfilePrefs(Profile* profile) {
  auto* pref_service = profile->GetPrefs();
  for (const auto& entry :
       pref_service->GetList(prefs::kNtpWallpaperSearchHistory)) {
    const auto entry_obj = GetHistoryEntryFromPrefValue(entry);
    if (entry_obj.has_value()) {
      DeleteWallpaperSearchImage(entry_obj->id.ToString(), profile->GetPath());
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
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kNtpWallpaperSearchHistory,
      base::BindRepeating(&WallpaperSearchBackgroundManager::NotifyAboutHistory,
                          weak_ptr_factory_.GetWeakPtr()));
}

WallpaperSearchBackgroundManager::~WallpaperSearchBackgroundManager() = default;

std::vector<HistoryEntry> WallpaperSearchBackgroundManager::GetHistory() {
  auto& history_list =
      pref_service_->GetList(prefs::kNtpWallpaperSearchHistory);
  std::vector<HistoryEntry> history;
  for (auto& entry : history_list) {
    const auto entry_obj = GetHistoryEntryFromPrefValue(entry);
    if (entry_obj) {
      history.push_back(*entry_obj);
    }
  }
  return history;
}

bool WallpaperSearchBackgroundManager::IsCurrentBackground(
    const base::Token& id) {
  std::optional<CustomBackground> current_theme =
      ntp_custom_background_service_->GetCustomBackground();
  return current_theme.has_value() &&
         current_theme->local_background_id.has_value() &&
         current_theme->local_background_id->ToString() == id.ToString();
}

void WallpaperSearchBackgroundManager::SelectHistoryImage(
    const base::Token& id,
    const gfx::Image& image,
    base::ElapsedTimer timer) {
  if (ntp_custom_background_service_->IsCustomBackgroundDisabledByPolicy() ||
      image.IsEmpty()) {
    return;
  }

  ntp_custom_background_service_->SetBackgroundToLocalResourceWithId(
      id, /*is_inspiration_image=*/false);
  ntp_custom_background_service_->UpdateCustomLocalBackgroundColorAsync(image);

  UmaHistogramMediumTimes(
      "NewTabPage.WallpaperSearch.SetRecentThemeProcessingLatency",
      timer.Elapsed());
}

void WallpaperSearchBackgroundManager::SelectLocalBackgroundImage(
    const base::Token& id,
    const SkBitmap& bitmap,
    bool is_inspiration_image,
    base::ElapsedTimer timer) {
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
    std::optional<CustomBackground> current_theme =
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
                         weak_ptr_factory_.GetWeakPtr(), id, std::move(timer),
                         bitmap, is_inspiration_image));
    } else {
      ntp_custom_background_service_->UpdateCustomLocalBackgroundColorAsync(
          gfx::Image::CreateFrom1xBitmap(bitmap));
    }
  }
}

std::optional<base::Token>
WallpaperSearchBackgroundManager::SaveCurrentBackgroundToHistory(
    const HistoryEntry& history_entry) {
  std::optional<CustomBackground> current_theme =
      ntp_custom_background_service_->GetCustomBackground();
  if (current_theme.has_value() &&
      current_theme->local_background_id.has_value() &&
      current_theme->local_background_id->ToString() ==
          history_entry.id.ToString()) {
    const base::Value::List& current_history =
        pref_service_->GetList(prefs::kNtpWallpaperSearchHistory);
    base::Value::List new_history =
        base::Value::List().Append(GetHistoryEntryDict(history_entry));
    // Add each value in |current_history| to |new_history| until
    // |new_history| reaches the max size of 6. Do not append the
    // value if it is the same as the id of |current_theme|.
    for (const auto& value : current_history) {
      const auto value_obj = GetHistoryEntryFromPrefValue(value);
      if (value_obj) {
        if (value_obj.value() != history_entry) {
          if (new_history.size() >= 6) {
            // Delete values that will no longer be in the history.
            DeleteWallpaperSearchImage(value_obj->id.ToString(),
                                       profile_->GetPath());
          } else {
            new_history.Append(GetHistoryEntryDict(value_obj.value()));
          }
        }
      }
    }
    pref_service_->SetList(prefs::kNtpWallpaperSearchHistory,
                           std::move(new_history));
    return history_entry.id;
  }
  return std::nullopt;
}

void WallpaperSearchBackgroundManager::AddObserver(
    WallpaperSearchBackgroundManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void WallpaperSearchBackgroundManager::RemoveObserver(
    WallpaperSearchBackgroundManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WallpaperSearchBackgroundManager::SetBackgroundToLocalResourceWithId(
    const base::Token& id,
    base::ElapsedTimer timer,
    const SkBitmap& bitmap,
    bool is_inspiration_image) {
  ntp_custom_background_service_->SetBackgroundToLocalResourceWithId(
      id, is_inspiration_image);
  ntp_custom_background_service_->UpdateCustomLocalBackgroundColorAsync(
      gfx::Image::CreateFrom1xBitmap(bitmap));
  UmaHistogramMediumTimes(
      is_inspiration_image
          ? "NewTabPage.WallpaperSearch.SetInspirationThemeProcessingLatency"
          : "NewTabPage.WallpaperSearch.SetResultThemeProcessingLatency",
      timer.Elapsed());
}

void WallpaperSearchBackgroundManager::NotifyAboutHistory() {
  for (auto& observer : observers_) {
    observer.OnHistoryUpdated();
  }
}
