// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_AVATAR_ICON_UTIL_H_
#define CHROME_BROWSER_PROFILES_PROFILE_AVATAR_ICON_UTIL_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/files/file_path.h"
#include "base/values.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image_skia.h"

namespace base {
class FilePath;
}

namespace gfx {
class Image;
}

class Profile;
class ProfileAttributesEntry;
class SkBitmap;

namespace profiles {

enum class AvatarVisibilityAgainstBackground {
  // Use a color for the icon that is visible against the background.
  kVisibleAgainstDarkTheme,
  kVisibleAgainstLightTheme
};

struct PlaceholderAvatarIconParams {
  bool has_padding = true;
  bool has_background = true;
  std::optional<AvatarVisibilityAgainstBackground>
      visibility_against_background;
};

#if BUILDFLAG(IS_WIN)
// The avatar badge size needs to be half of the shortcut icon size because
// the Windows taskbar icon is 32x32 and the avatar icon overlay is 16x16. So to
// get the shortcut avatar badge and the avatar icon overlay to match up, we
// need to preserve those ratios when creating the shortcut icon.
const int kShortcutIconSizeWin = 48;
const int kProfileAvatarBadgeSizeWin = kShortcutIconSizeWin / 2;
#endif  // BUILDFLAG(IS_WIN)

// Size of the small identity images for list of profiles to switch to.
constexpr int kMenuAvatarIconSize = 20;

// Avatar access.
extern const base::FilePath::CharType kGAIAPictureFileName[];
extern const base::FilePath::CharType kHighResAvatarFolderName[];

// Avatar formatting.
extern const int kAvatarIconSize;
extern const SkColor kAvatarTutorialBackgroundColor;
extern const SkColor kAvatarTutorialContentTextColor;
extern const SkColor kAvatarBubbleAccountsBackgroundColor;
extern const SkColor kAvatarBubbleGaiaBackgroundColor;
extern const SkColor kUserManagerBackgroundColor;

// Avatar shape.
enum AvatarShape {
  SHAPE_CIRCLE,  // Only available for desktop platforms
  SHAPE_SQUARE,
};

// Returns the default guest avatar.
ui::ImageModel GetGuestAvatar(int size = 256);

// Returns a version of |image| of a specific size. Note that no checks are
// done on the width/height so make sure they're reasonable values; in the
// range of 16-256 is probably best.
gfx::Image GetSizedAvatarIcon(const gfx::Image& image,
                              int width,
                              int height,
                              AvatarShape shape);

gfx::Image GetSizedAvatarIcon(const gfx::Image& image, int width, int height);

// Returns a version of |image| suitable for use in WebUI.
gfx::Image GetAvatarIconForWebUI(const gfx::Image& image);

// Returns a version of |image| suitable for use in title bars. The returned
// image is scaled to fit |dst_width| and |dst_height|.
gfx::Image GetAvatarIconForTitleBar(const gfx::Image& image,
                                    int dst_width,
                                    int dst_height);

#if BUILDFLAG(IS_MAC)
// Returns the image for the profile at |profile_path| that is suitable for use
// in the macOS menu bar.
gfx::Image GetAvatarIconForNSMenu(const base::FilePath& profile_path);
#endif

// Gets the number of default avatar icons that exist.
size_t GetDefaultAvatarIconCount();

// Gets the number of generic avatar icons that exist.
size_t GetGenericAvatarIconCount();

// Gets the index for the (grey silhouette) avatar used as a placeholder.
size_t GetPlaceholderAvatarIndex();

// Gets the start index of the modern profile avatar icons.
size_t GetModernAvatarIconStartIndex();

// Returns whether |icon_index| corresponds to one of the modern profile avatar
// icons.
bool IsModernAvatarIconIndex(size_t icon_index);

// Gets the resource ID of the placeholder avatar icon.
int GetPlaceholderAvatarIconResourceID();

// Returns a URL for the placeholder avatar icon.
std::string GetPlaceholderAvatarIconUrl();

// Returns the outline silhouette colored generic avatar, either visible against
// a dark or a light theme background. This function is currently under
// experiment and only used when `kOutlineSilhouetteIcon` is enabled.
gfx::Image GetPlaceholderAvatarIconVisibleAgainstBackground(
    SkColor profile_color_seed,
    int size,
    AvatarVisibilityAgainstBackground visibility);

// Returns a filled person icon if `kOutlineSilhouetteIcon` is disabled, and the
// outline silhouette colored generic avatar if it is enabled. Depending on the
// `icon_params`, the outline silhouette avatar will have a background/padding
// or not.
//
// If the avatar icon should not have a background itself but be visible against
// the background it is displayed against, use
// `GetPlaceholderAvatarIconVisibleAgainstBackground()` instead.
gfx::Image GetPlaceholderAvatarIconWithColors(
    SkColor fill_color,
    SkColor stroke_color,
    int size,
    const PlaceholderAvatarIconParams& icon_params = {});

// Gets the resource ID of the default avatar icon at |index|.
int GetDefaultAvatarIconResourceIDAtIndex(size_t index);

#if BUILDFLAG(IS_WIN)
// Gets the resource ID of the 2x sized version of the old profile avatar icon
// at |index|.
int GetOldDefaultAvatar2xIconResourceIDAtIndex(size_t index);
#endif  // BUILDFLAG(IS_WIN)

// Gets the resource filename of the default avatar icon at |index|.
const char* GetDefaultAvatarIconFileNameAtIndex(size_t index);

// Gets the resource ID of the default avatar label at |index|.
int GetDefaultAvatarLabelResourceIDAtIndex(size_t index);

// Gets the full path of the high res avatar icon at |index|.
base::FilePath GetPathOfHighResAvatarAtIndex(size_t index);

// Returns a URL for the default avatar icon with specified index.
std::string GetDefaultAvatarIconUrl(size_t index);

// Checks if |index| is a valid avatar icon index
bool IsDefaultAvatarIconIndex(size_t index);

// Checks if the given URL points to one of the default avatar icons. If it
// is, returns true and its index through |icon_index|. If not, returns false.
bool IsDefaultAvatarIconUrl(const std::string& icon_url, size_t *icon_index);

// Returns dictionary containing the avatar icon info in the format expected by
// the WebUI component 'cr-profile-avatar-selector'.
base::Value::Dict GetAvatarIconAndLabelDict(const std::string& url,
                                            const std::u16string& label,
                                            size_t index,
                                            bool selected,
                                            bool is_gaia_avatar);

// Returns dictionary containing the default generic avatar icon, label, index
// and selected state.
base::Value::Dict GetDefaultProfileAvatarIconAndLabel(SkColor fill_color,
                                                      SkColor stroke_color,
                                                      bool selected);

// Returns a list of dictionaries containing modern profile avatar icons as
// well as avatar labels used for accessibility purposes. The list is ordered
// according to the avatars' default order. If |selected_avatar_idx| is one of
// the available indices, the corresponding avatar is marked as selected.
base::Value::List GetCustomProfileAvatarIconsAndLabels(
    size_t selected_avatar_idx = SIZE_MAX);

// This method tries to find a random avatar index that is not in
// |used_icon_indices|. If there is no such index, a random index is returned.
size_t GetRandomAvatarIconIndex(
    const std::unordered_set<size_t>& used_icon_indices);

#if !BUILDFLAG(IS_ANDROID)
// Get all the available profile icons to choose from for a specific profile
// with |profile_path|.
base::Value::List GetIconsAndLabelsForProfileAvatarSelector(
    const base::FilePath& profile_path);
#endif  // !BUILDFLAG(IS_ANDROID)

// Set the default profile avatar icon index to |avatar_icon_index| for a
// specific |profile|.
void SetDefaultProfileAvatarIndex(Profile* profile, size_t avatar_icon_index);

#if BUILDFLAG(IS_WIN)
// Get the 2x avatar image for a ProfileAttributesEntry.
SkBitmap GetWin2xAvatarImage(ProfileAttributesEntry* entry);

// Returns a bitmap with a couple of columns shaved off so it is more square,
// so that when resized to a square aspect ratio it looks pretty.
SkBitmap GetWin2xAvatarIconAsSquare(const SkBitmap& source_bitmap);

// Badges |app_icon_bitmap| with |avatar_bitmap| at the bottom right corner and
// returns the resulting SkBitmap.
SkBitmap GetBadgedWinIconBitmapForAvatar(const SkBitmap& app_icon_bitmap,
                                         const SkBitmap& avatar_bitmap);
#endif  // BUILDFLAG(IS_WIN)

}  // namespace profiles

#endif  // CHROME_BROWSER_PROFILES_PROFILE_AVATAR_ICON_UTIL_H_
