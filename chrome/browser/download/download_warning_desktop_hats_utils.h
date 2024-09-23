// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_WARNING_DESKTOP_HATS_UTILS_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_WARNING_DESKTOP_HATS_UTILS_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "components/download/public/common/download_item.h"

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
    // Timeout used for "ignore" survey trigger. Only added for a download
    // bubble warning that was ignored.
    static constexpr char kIgnoreTimeoutSeconds[] =
        "Threshold for ignored warning (s)";
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

// A class that manages delayed download warning HaTS survey tasks. It can be
// given a DownloadItem to launch a survey for in the future after some delay,
// and these tasks can be canceled explicitly or automatically (in case of
// the DownloadItem getting destroyed or becoming ineligible for a HaTS survey).
// It also records the last time the user interacted with the browser, and the
// survey is withheld if the user was (presumably) idle for the entire period of
// the delay. (Client should inform this object of browser activity.)
// Note: Currently this is only used for download bubble ignore triggers.
class DelayedDownloadWarningHatsLauncher
    : public download::DownloadItem::Observer {
 public:
  // A callback that allows the completion of the PSD (addition of post-Create()
  // fields).
  using PsdCompleter =
      base::RepeatingCallback<void(DownloadWarningHatsProductSpecificData&)>;

  // Bundles the objects used to control the task and its lifetime. Can only be
  // used once per instance. To cancel, delete this object. The `download`
  // and `hats_launcher` must outlive this.
  class Task {
   public:
    // Creates and schedules the task.
    Task(DelayedDownloadWarningHatsLauncher& hats_launcher,
         download::DownloadItem* download,
         base::OnceClosure task,
         base::TimeDelta delay);

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    ~Task();

   private:
    void RunTask();

    // Controls the observation of the download by the parent object.
    base::ScopedObservation<download::DownloadItem,
                            DelayedDownloadWarningHatsLauncher>
        observation_;
    // Task to show the survey.
    base::OnceClosure task_;
    // Used to cancel the scheduled task.
    base::WeakPtrFactory<Task> weak_factory_{this};
  };

  // `profile` is the profile for which the HaTS surveys should be shown.
  // `delay` is the delay that applies to all surveys launched by this object.
  // `psd_completer` will be called with the product-specific data right before
  // attempting to launch each survey.
  DelayedDownloadWarningHatsLauncher(
      Profile* profile,
      base::TimeDelta delay,
      PsdCompleter psd_completer = base::DoNothing());

  DelayedDownloadWarningHatsLauncher(
      const DelayedDownloadWarningHatsLauncher&) = delete;
  DelayedDownloadWarningHatsLauncher& operator=(
      const DelayedDownloadWarningHatsLauncher&) = delete;

  ~DelayedDownloadWarningHatsLauncher() override;

  // download::DownloadItem::Observer:
  // This object is an observer of every download with an entry in `tasks_`.
  void OnDownloadUpdated(download::DownloadItem* download) override;
  void OnDownloadDestroyed(download::DownloadItem* download) override;

  // Updates the last_activity_ time.
  void RecordBrowserActivity();

  // Schedules a survey to be shown after the delay, if the user has been active
  // in the meantime. Does nothing if a task already exists for the download.
  // Does nothing if the download is not eligible when scheduling. (The
  // scheduled task will also fizzle if the download is not eligible upon
  // execution.) Returns whether task was scheduled.
  bool TryScheduleTask(DownloadWarningHatsType survey_type,
                       download::DownloadItem* download);

  // Cancels and removes the task for `download` from the map. Is a no-op if the
  // download is not in the map.
  void RemoveTaskIfAny(download::DownloadItem* download);

 private:
  // Address of a DownloadItem, derived from a DownloadItem*, but it is not to
  // be dereferenced.
  using TaskKey = std::uintptr_t;

  // Returns the task key to be used for the download, which is just the
  // address.
  TaskKey GetTaskKey(download::DownloadItem* download);

  // Cancels and removes the task from the map. Is a no-op if the key is not in
  // the map. In particular, if the DownloadItem has been freed, its key will
  // not be found in the map, as guaranteed by the DownloadItem::Observer
  // mechanism.
  void RemoveTaskByKeyIfAny(TaskKey key);

  // Launches the actual survey, if all preconditions are met.
  void MaybeLaunchSurveyNow(DownloadWarningHatsType survey_type,
                            download::DownloadItem* download);

  // Returns a callback that is called to clean up after the survey succeeds or
  // fails.
  base::OnceClosure MakeSurveyDoneCallback(download::DownloadItem* download);

  // Whether the user was active in the browser during the delay period.
  bool WasUserActive() const;

  // Profile to show the surveys for. Must outlive this.
  const raw_ptr<Profile> profile_;
  // How long to wait before launching the survey.
  const base::TimeDelta delay_;
  // Time of the most recent user interaction with the browser.
  base::Time last_activity_;
  // Maps DownloadItem addresses to their corresponding pending tasks.
  std::map<TaskKey, Task> tasks_;
  // Callback that is run to stamp the PSD with any additional fields right
  // before attempting to launch the survey.
  PsdCompleter psd_completer_;
  // Needed because the cleanup callback produced by MakeSurveyDoneCallback
  // may outlive this.
  base::WeakPtrFactory<DelayedDownloadWarningHatsLauncher> weak_factory_{this};
};

// Returns if the download item is dangerous and not-done.
bool CanShowDownloadWarningHatsSurvey(download::DownloadItem* download);

// Returns the HaTS trigger string for the survey_type, if the user is eligible
// for that type of survey (according to the fieldtrial config). If the user
// is not eligible, or there is a configuration error, this returns nullopt.
std::optional<std::string> MaybeGetDownloadWarningHatsTrigger(
    DownloadWarningHatsType survey_type);

// Returns the time delay used for kDownloadBubbleIgnore triggers.
base::TimeDelta GetIgnoreDownloadBubbleWarningDelay();

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
    const DownloadWarningHatsProductSpecificData& psd,
    base::OnceClosure success_callback = base::DoNothing(),
    base::OnceClosure failure_callback = base::DoNothing());

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_WARNING_DESKTOP_HATS_UTILS_H_
