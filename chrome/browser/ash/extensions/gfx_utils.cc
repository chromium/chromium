// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/gfx_utils.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_extension_constants.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/grit/chromeos_app_icon_resources.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace extensions {

namespace {

// The badge map between |arc_package_name| and |extension_id|. Note the mapping
// from |extension_id| to |arc_package_name| is unique, but the mapping from
// |arc_package_name| to |extension_id| is not.
constexpr std::string_view kGooglePlayBooksExtensionIds[] = {
    extension_misc::kGooglePlayBooksAppId,
};
constexpr std::string_view kGoogleClassroomExtensionIds[] = {
    "mfhehppjhmmnlfbbopchdfldgimhfhfk",
};
constexpr std::string_view kGoogleDriveExtensionIds[] = {
    extension_misc::kGoogleDriveAppId,
    "mdhnphfgagkpdhndljccoackjjhghlif",
};
constexpr std::string_view kGoogleDocsExtensionIds[] = {
    extension_misc::kGoogleDocsAppId,
    "npnjdccdffhdndcbeappiamcehbhjibf",
};
constexpr std::string_view kGoogleSheetsExtensionIds[] = {
    extension_misc::kGoogleSheetsAppId,
    "nifkmgcdokhkjghdlgflonppnefddien",
};
constexpr std::string_view kGoogleSlidesExtensionIds[] = {
    extension_misc::kGoogleSlidesAppId,
    "hdmobeajeoanbanmdlabnbnlopepchip",
};
constexpr std::string_view kGoogleNewsExtensionIds[] = {
    "dllkocilcinkggkchnjgegijklcililc",
};
constexpr std::string_view kGoogleMapsExtensionIds[] = {
    "lneaknkopdijkpnocmklfnjbeapigfbh",
};
constexpr std::string_view kGooglePhotosExtensionIds[] = {
    "hcglmfcclpfgljeaiahehebeoaiicbko",
    "efjnaogkjbogokcnohkmnjdojkikgobo",
    "ifpkhncdnjfipfjlhfidljjffdgklanh",
};
constexpr std::string_view kGooglePlusExtensionIds[] = {
    "dlppkpafhbajpcmmoheippocdidnckmm",
    "fgjnkhlabjcaajddbaenilcmpcidahll",
};
constexpr std::string_view kCalculatorExtensionIds[] = {
    "joodangkbfjnajiiifokapkpmhfnpleo",
};
constexpr std::string_view kGoogleCalendarExtensionIds[] = {
    "ejjicmeblgpmajnghnpcppodonldlgfn",
    "fpgfohogebplgnamlafljlcidjedbdeb",
};
constexpr std::string_view kGmailExtensionIds[] = {
    extension_misc::kGmailAppId,
    "bjdhhokmhgelphffoafoejjmlfblpdha",
};
constexpr std::string_view kGoogleKeepExtensionIds[] = {
    "hmjkmjkepdijhoojdojkdfohbdgmmhki",
    "dondgdlndnpianbklfnehgdhkickdjck",
};
constexpr std::string_view kGoogleNowExtensionIds[] = {
    "mnfadmojomeniojkkikjpgjaegolkbpb",
};
constexpr std::string_view kGooglePlayMusicExtensionIds[] = {
    extension_misc::kGooglePlayMusicAppId,
    "onbhgdmifjebcabplolilidlpgeknifi",
    "fahmaaghhglfmonjliepjlchgpgfmobi",
};
constexpr std::string_view kGoogleHangoutsExtensionIds[] = {
    "knipolnnllmklapflnccelgolnpehhpl",
    "cgmlfbhkckbedohgdepgbkflommbfkep",
    "gldgpnmcpaogjlojhhpebkbbanacoglc",
};
constexpr std::string_view kGooglePlayMoviesExtensionIds[] = {
    extension_misc::kGooglePlayMoviesAppId,
    "amfoiggnkefambnaaphodjdmdooiinna",
};
constexpr std::string_view kYoutubeExtensionIds[] = {
    extension_misc::kYoutubeAppId,
    "pbdihpaifchmclcmkfdgffnnpfbobefh",
};
// Used in unit tests.
constexpr std::string_view kFakePackageExtensionIds[] = {
    "emfkafnhnpcmabnnkckkchdilgeoekbo",
};

constexpr auto kArcAppToExtensionsMap =
    base::MakeFixedFlatMap<std::string_view,
                           base::span<const std::string_view>>({
        // Google Play Books
        {"com.google.android.apps.books", kGooglePlayBooksExtensionIds},
        // Google Classroom
        {"com.google.android.apps.classroom", kGoogleClassroomExtensionIds},
        // Google Drive
        {"com.google.android.apps.docs", kGoogleDriveExtensionIds},
        // Google Docs
        {"com.google.android.apps.docs.editors.docs", kGoogleDocsExtensionIds},
        // Google Sheets
        {"com.google.android.apps.docs.editors.sheets",
         kGoogleSheetsExtensionIds},
        // Google Slides
        {"com.google.android.apps.docs.editors.slides",
         kGoogleSlidesExtensionIds},
        // Google News
        {"com.google.android.apps.genie.geniewidget", kGoogleNewsExtensionIds},
        // Google Maps
        {"com.google.android.apps.maps", kGoogleMapsExtensionIds},
        // Google Photos
        {"com.google.android.apps.photos", kGooglePhotosExtensionIds},
        // Google+
        {"com.google.android.apps.plus", kGooglePlusExtensionIds},
        // Calculator
        {"com.google.android.calculator", kCalculatorExtensionIds},
        // Google Calendar
        {"com.google.android.calendar", kGoogleCalendarExtensionIds},
        // GMail
        {"com.google.android.gm", kGmailExtensionIds},
        // Google Keep
        {"com.google.android.keep", kGoogleKeepExtensionIds},
        // Google Now
        {"com.google.android.launcher", kGoogleNowExtensionIds},
        // Google Play Music
        {"com.google.android.music", kGooglePlayMusicExtensionIds},
        // Google Hangouts
        {"com.google.android.talk", kGoogleHangoutsExtensionIds},
        // Google Play Movies & TV
        {"com.google.android.videos", kGooglePlayMoviesExtensionIds},
        // YouTube
        {"com.google.android.youtube", kYoutubeExtensionIds},
        // Used in unit tests.
        {"fake.package.name1", kFakePackageExtensionIds},
    });

consteval auto MakeExtensionToArcAppMap() {
  constexpr size_t kTotalExtensions = [] {
    size_t count = 0;
    for (const auto& [arc_package, extensions] : kArcAppToExtensionsMap) {
      count += extensions.size();
    }
    return count;
  }();
  std::pair<std::string_view, std::string_view> entries[kTotalExtensions];
  base::span entries_span(entries);
  size_t i = 0;
  for (const auto& [arc_package, extensions] : kArcAppToExtensionsMap) {
    for (std::string_view extension_id : extensions) {
      entries_span[i++] = {extension_id, arc_package};
    }
  }
  return base::MakeFixedFlatMap(std::move(entries));
}

constexpr auto kExtensionToArcAppMap = MakeExtensionToArcAppMap();

constexpr base::span<const std::string_view> GetExtensionIdsForArcPackageName(
    std::string_view arc_package_name) {
  const auto iter = kArcAppToExtensionsMap.find(arc_package_name);
  if (iter == kArcAppToExtensionsMap.end()) {
    return {};
  }
  return iter->second;
}

constexpr std::string_view GetArcPackageNameFromExtensionId(
    std::string_view extension_id) {
  const auto iter = kExtensionToArcAppMap.find(extension_id);
  if (iter == kExtensionToArcAppMap.end()) {
    return {};
  }
  return iter->second;
}

}  // namespace

namespace util {

bool HasEquivalentInstalledArcApp(content::BrowserContext* context,
                                  const std::string& extension_id) {
  absl::flat_hash_set<std::string> arc_apps;
  return GetEquivalentInstalledArcApps(context, extension_id, &arc_apps);
}

bool GetEquivalentInstalledArcApps(content::BrowserContext* context,
                                   const std::string& extension_id,
                                   absl::flat_hash_set<std::string>* arc_apps) {
  std::string_view arc_package_name =
      GetArcPackageNameFromExtensionId(extension_id);
  if (arc_package_name.empty()) {
    return false;
  }

  const ArcAppListPrefs* const prefs = ArcAppListPrefs::Get(context);
  if (!prefs) {
    return false;
  }

  // TODO(hidehiko): The icon is per launcher, so we should have more precise
  // check here.
  CHECK(arc_apps, base::NotFatalUntil::M160);
  prefs->GetAppsForPackage(std::string(arc_package_name)).swap(*arc_apps);
  return !arc_apps->empty();
}

std::vector<std::string> GetEquivalentInstalledExtensions(
    content::BrowserContext* context,
    const std::string& arc_package_name) {
  const ExtensionRegistry* registry = ExtensionRegistry::Get(context);
  if (!registry) {
    return {};
  }

  std::vector<std::string> extension_ids;
  for (std::string_view extension_id :
       GetExtensionIdsForArcPackageName(arc_package_name)) {
    std::string id(extension_id);
    if (registry->GetInstalledExtension(id)) {
      extension_ids.push_back(std::move(id));
    }
  }
  return extension_ids;
}

bool ShouldApplyChromeBadge(content::BrowserContext* context,
                            const std::string& extension_id) {
  CHECK(context, base::NotFatalUntil::M160);

  Profile* profile = Profile::FromBrowserContext(context);
  // Only apply Chrome badge for the primary profile.
  if (!ash::ProfileHelper::IsPrimaryProfile(profile) ||
      !multi_user_util::IsProfileFromActiveUser(profile)) {
    return false;
  }

  const ExtensionRegistry* registry = ExtensionRegistry::Get(context);
  if (!registry || !registry->GetInstalledExtension(extension_id))
    return false;

  if (!HasEquivalentInstalledArcApp(context, extension_id))
    return false;

  return true;
}

bool ShouldApplyChromeBadgeToWebApp(content::BrowserContext* context,
                                    const std::string& web_app_id) {
  CHECK(context, base::NotFatalUntil::M160);

  Profile* profile = Profile::FromBrowserContext(context);
  // Only apply Chrome badge for the primary profile.
  if (!ash::ProfileHelper::IsPrimaryProfile(profile) ||
      !multi_user_util::IsProfileFromActiveUser(profile)) {
    return false;
  }

  if (!HasEquivalentInstalledArcApp(context, web_app_id))
    return false;

  return true;
}

void ApplyBadge(gfx::ImageSkia* icon_out, ChromeAppIcon::Badge badge_type) {
  CHECK(icon_out, base::NotFatalUntil::M160);
  CHECK_NE(ChromeAppIcon::Badge::kNone, badge_type, base::NotFatalUntil::M160);

  int badge_res = 0;
  switch (badge_type) {
    case ChromeAppIcon::Badge::kChrome:
      badge_res = IDR_CHROME_ICON_BADGE;
      break;
    case ChromeAppIcon::Badge::kBlocked:
      badge_res = IDR_BLOCK_ICON_BADGE;
      break;
    case ChromeAppIcon::Badge::kPaused:
      badge_res = IDR_HOURGLASS_ICON_BADGE;
      break;
    default:
      NOTREACHED();
  }

  const gfx::ImageSkia* badge_image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(badge_res);
  CHECK(badge_image, base::NotFatalUntil::M160);

  gfx::ImageSkia resized_badge_image = *badge_image;
  if (badge_image->size() != icon_out->size()) {
    resized_badge_image = gfx::ImageSkiaOperations::CreateResizedImage(
        *badge_image, skia::ImageOperations::RESIZE_BEST, icon_out->size());
  }
  *icon_out = gfx::ImageSkiaOperations::CreateSuperimposedImage(
      *icon_out, resized_badge_image);
}

}  // namespace util
}  // namespace extensions
