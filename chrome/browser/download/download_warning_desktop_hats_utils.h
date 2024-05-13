// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_WARNING_DESKTOP_HATS_UTILS_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_WARNING_DESKTOP_HATS_UTILS_H_

#include <optional>
#include <string>
#include <vector>

#include "chrome/browser/ui/hats/hats_service.h"

namespace download {
class DownloadItem;
}

// Type of survey (corresponding to a trigger condition) that should be shown.
// Do not renumber.
enum class DownloadWarningHatsType {
  // Kept a suspicious file from the download bubble.
  kDownloadBubbleBypass = 0,
  // Deleted a warned file from the download bubble.
  kDownloadBubbleHeed = 1,
  // User did not act on warning despite being active.
  kDownloadBubbleIgnore = 2,
  // Kept a suspicious/dangerous file from the downloads page or the download
  // danger prompt.
  kDownloadsPageBypass = 3,
  // Deleted a warned file from the downloads page.
  kDownloadsPageHeed = 4,
  // Navigated away from or closed the downloads page without acting on the
  // warning.
  kDownloadsPageIgnore = 5,
  kMaxValue = kDownloadsPageIgnore,
};

// Stores the PSD for the download warning HaTS survey.
class DownloadWarningHatsProductSpecificData {
 public:
  // A namespace for the field labels, which are presented to the user in the
  // privacy and transparency UI.
  struct Fields {
    // Bits data fields:

    // Whether the download bubble partial view was enabled.
    static constexpr char kPartialViewEnabled[] =
        "Automatic download bubble enabled (\"Show downloads when they're "
        "done\" setting)";
    // Whether the user interacted with the partial view (true) or main view
    // (false). Only logged for download bubble triggers. Must be added after
    // Create(). Defaults to false if no value is added.
    static constexpr char kPartialViewInteraction[] =
        "User interacted with automatic download bubble";
    // Whether the download was initiated by a user gesture.
    static constexpr char kUserGesture[] = "Download initiated by user gesture";

    // String data fields:

    // The outcome of the download: "Bypassed warning", "Heeded warning", or
    // "Ignored warning".
    static constexpr char kOutcome[] = "Outcome";
    // The UI surface on which the outcome occurred: "Download bubble" or
    // "Downloads page".
    static constexpr char kSurface[] = "UI surface";
    // A combination of danger type of the download and tailored verdict (if
    // any; see csd.proto).
    static constexpr char kDangerType[] = "Danger type";
    // Danger UI pattern: "Dangerous", "Suspicious", or "None".
    static constexpr char kWarningType[] = "Warning type";
    // What the user's Safe Browsing setting is.
    static constexpr char kSafeBrowsingState[] = "Safe Browsing state";
    // Client channel, "Stable", "Beta", etc.
    static constexpr char kChannel[] = "Chrome channel";
    // How many total warned downloads were on the downloads page. Only logged
    // for downloads page triggers. Must be added after Create(). Defaults to
    // a placeholder if no value is added.
    static constexpr char kNumPageWarnings[] =
        "Number of warnings on downloads page";
    // The user's interactions with this specific warning, as a
    // comma-separated string of timestamped actions.
    // Only logged for users with Enhanced Safe Browsing.
    // Other users will have a placeholder value.
    static constexpr char kWarningInteractions[] =
        "User interactions with this download warning with timestamps (ms)";
    // The time elapsed since the download started, in seconds.
    static constexpr char kSecondsSinceDownloadStarted[] =
        "Time since download started (s)";
    // The time elapsed since the warning was shown, in seconds.
    static constexpr char kSecondsSinceWarningShown[] =
        "Time since warning shown (s)";
    // URLs of the downloaded file and referring page.
    // These are only logged for users with Safe Browsing enabled.
    // Other users will have a placeholder value.
    static constexpr char kUrlDownload[] = "Download URL";
    static constexpr char kUrlReferrer[] = "Referrer URL";
    // Download filename.
    // Only logged for users with Safe Browsing enabled.
    // Other users will have a placeholder value.
    static constexpr char kFilename[] = "Download filename";
    // TODO(chlily): Add kIgnoreTimeout.
    // Timeout used for "ignore" survey trigger. Only added if the outcome was
    // ignore.
  };

  // Returns a ProductSpecificData with some basic PSD values filled in.
  // This only fills in certain fields derivable from the DownloadItem and the
  // profile. Other fields may need to be supplied by the caller directly.
  // Note: Caller must ensure that the DownloadItem is dangerous and not done.
  static DownloadWarningHatsProductSpecificData Create(
      DownloadWarningHatsType survey_type,
      download::DownloadItem* download_item);

  // Methods to add PSD fields that the caller supplies:
  // Must be called for any downloads page trigger before sending the survey.
  void AddNumPageWarnings(int num);
  // Must be called for any download bubble trigger before sending the survey.
  void AddPartialViewInteraction(bool partial_view_interaction);

  // Returns the names of the PSD fields, used in creating the survey configs.
  // These are CHECKed against the fields ultimately passed to the HaTS
  // service.
  static std::vector<std::string> GetBitsDataFields(
      DownloadWarningHatsType survey_type);
  static std::vector<std::string> GetStringDataFields(
      DownloadWarningHatsType survey_type);

  // Note that the applicable fields must all be present before using this
  // object, so AddNumPageWarnings or AddPartialViewInteraction must have been
  // called, otherwise using this object will cause a CHECK failure.
  const SurveyBitsData& bits_data() const { return bits_data_; }
  const SurveyStringData& string_data() const { return string_data_; }

  DownloadWarningHatsType survey_type() const { return survey_type_; }

  DownloadWarningHatsProductSpecificData(
      const DownloadWarningHatsProductSpecificData&);
  DownloadWarningHatsProductSpecificData& operator=(
      const DownloadWarningHatsProductSpecificData&);
  DownloadWarningHatsProductSpecificData(
      DownloadWarningHatsProductSpecificData&&);
  DownloadWarningHatsProductSpecificData& operator=(
      DownloadWarningHatsProductSpecificData&&);

  ~DownloadWarningHatsProductSpecificData();

 private:
  explicit DownloadWarningHatsProductSpecificData(
      DownloadWarningHatsType survey_type);

  DownloadWarningHatsType survey_type_;

  SurveyBitsData bits_data_;
  SurveyStringData string_data_;
};

// Returns if the download item is dangerous and not-done.
bool CanShowDownloadWarningHatsSurvey(download::DownloadItem* download);

// Returns the HaTS trigger string for the survey_type, if the user is eligible
// for that type of survey (according to the fieldtrial config). If the user
// is not eligible, or there is a configuration error, this returns nullopt.
std::optional<std::string> MaybeGetDownloadWarningHatsTrigger(
    DownloadWarningHatsType survey_type);

// Launches a HaTS survey using the desktop HaTS service, if all preconditions
// are met. The `psd` object encapsulates the data for the survey, including the
// triggering survey type. `profile` is the profile for which the survey should
// be launched. Note that it is potentially different from the profile under
// which the download was made (in the case of OTR profiles which may care about
// downloads made in their original profile), so it needs to be passed and
// cannot be derived from the DownloadItem. However, when `profile` is OTR and
// differs from the DownloadItem's Profile, a HaTS survey won't be shown anyway
// because HaTS surveys are not shown for OTR profiles, so everything is fine
// as long as we pass the correct `profile` for which we are attempting to
// launch the survey.
void MaybeLaunchDownloadWarningHatsSurvey(
    Profile* profile,
    const DownloadWarningHatsProductSpecificData& psd);

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_WARNING_DESKTOP_HATS_UTILS_H_
