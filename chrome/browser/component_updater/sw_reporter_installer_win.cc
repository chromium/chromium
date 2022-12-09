// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/sw_reporter_installer_win.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/reporter_runner_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/common/channel_info.h"
#include "components/chrome_cleaner/public/constants/buildflags.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/update_client.h"
#include "components/update_client/utils.h"
#include "components/version_info/channel.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// For ::GetTickCount()
#include <windows.h>

namespace component_updater {

namespace {

using safe_browsing::SwReporterInvocation;
using safe_browsing::SwReporterInvocationSequence;

// CRX hash. The extension id is: gkmgaooipdjhmangpemjhigmamcehddo. The hash was
// generated in Python with something like this:
// hashlib.sha256().update(open("<file>.crx").read()[16:16+294]).digest().
const uint8_t kSwReporterSha2Hash[] = {
    0x6a, 0xc6, 0x0e, 0xe8, 0xf3, 0x97, 0xc0, 0xd6, 0xf4, 0xc9, 0x78,
    0x6c, 0x0c, 0x24, 0x73, 0x3e, 0x05, 0xa5, 0x62, 0x4b, 0x2e, 0xc7,
    0xb7, 0x1c, 0x5f, 0xea, 0xf0, 0x88, 0xf6, 0x97, 0x9b, 0xc7};

const base::FilePath::CharType kSwReporterExeName[] =
    FILE_PATH_LITERAL("software_reporter_tool.exe");

// SwReporter is normally only registered in Chrome branded builds. However, to
// enable testing in chromium build bots, test code can set this to true.
#if BUILDFLAG(ENABLE_SOFTWARE_REPORTER)
bool is_sw_reporter_enabled = true;
#else
bool is_sw_reporter_enabled = false;
#endif

// Callback function to be called once the registration of the component
// is complete.  This is used only in tests.
base::OnceClosure& GetRegistrationCBForTesting() {
  static base::NoDestructor<base::OnceClosure> registration_cb_for_testing;
  return *registration_cb_for_testing;
}

void ReportUploadsWithUma(const std::u16string& upload_results) {
  base::String16Tokenizer tokenizer(upload_results, u";");
  bool last_result = false;
  while (tokenizer.GetNext()) {
    last_result = (tokenizer.token_piece() != u"0");
  }

  UMA_HISTOGRAM_BOOLEAN("SoftwareReporter.LastUploadResult", last_result);
}

void ReportConfigurationError(SoftwareReporterConfigurationError error) {
  UMA_HISTOGRAM_ENUMERATION("SoftwareReporter.ConfigurationErrors", error);
}

// Ensures |str| contains only alphanumeric characters and characters from
// |extras|, and is not longer than |max_length|.
bool ValidateString(const std::string& str,
                    const std::string& extras,
                    size_t max_length) {
  return str.size() <= max_length &&
         base::ranges::all_of(str, [&extras](char c) {
           return base::IsAsciiAlpha(c) || base::IsAsciiDigit(c) ||
                  extras.find(c) != std::string::npos;
         });
}

std::string GenerateSessionId() {
  std::string session_id;
  base::Base64Encode(base::RandBytesAsString(30), &session_id);
  DCHECK(!session_id.empty());
  return session_id;
}

// Add |behaviour_flag| to |supported_behaviours| if |behaviour_name| is found
// in the dictionary. Returns false on error.
bool GetOptionalBehaviour(
    const base::Value::Dict& invocation_params,
    base::StringPiece behaviour_name,
    SwReporterInvocation::Behaviours behaviour_flag,
    SwReporterInvocation::Behaviours* supported_behaviours) {
  DCHECK(supported_behaviours);

  // Parameters enabling behaviours are optional, but if present must be
  // boolean.
  const base::Value* value = invocation_params.Find(behaviour_name);
  if (value) {
    const absl::optional<bool> enabled = value->GetIfBool();
    if (!enabled.has_value()) {
      ReportConfigurationError(kBadParams);
      return false;
    }
    if (*enabled)
      *supported_behaviours |= behaviour_flag;
  }
  return true;
}

// Reads the command-line params and an UMA histogram suffix from the manifest
// and schedules an invocation sequence of the software reporter if successful.
void ScheduleSoftwareReporterWithManifest(
    const base::FilePath& exe_path,
    const base::Version& version,
    base::Value::Dict manifest,
    OnComponentReadyCallback ready_callback) {
  safe_browsing::SwReporterInvocationSequence invocations(version);

  const std::string* prompt_seed = manifest.FindString("prompt_seed");
  if (!prompt_seed) {
    ReportConfigurationError(kMissingPromptSeed);
    return;
  }

  // Allow an empty or missing "launch_params" list, but log an error if it
  // cannot be parsed as a list.
  const base::Value::List* parameter_list = nullptr;
  const base::Value* launch_params = manifest.Find("launch_params");
  if (launch_params) {
    parameter_list = launch_params->GetIfList();
    if (!parameter_list) {
      ReportConfigurationError(kBadParams);
      return;
    }
  }

  // Use a random session id to link reporter invocations together.
  const std::string session_id = GenerateSessionId();

  // If there are no launch parameters, create a single invocation with default
  // behaviour.
  if (!parameter_list || parameter_list->empty()) {
    base::CommandLine command_line(exe_path);
    command_line.AppendSwitchASCII(chrome_cleaner::kSessionIdSwitch,
                                   session_id);
    invocations.PushInvocation(
        SwReporterInvocation(command_line)
            .WithSupportedBehaviours(
                SwReporterInvocation::BEHAVIOURS_ENABLED_BY_DEFAULT));
    ready_callback.Run(*prompt_seed, std::move(invocations));
    return;
  }

  for (const base::Value& entry : *parameter_list) {
    const base::Value::Dict* invocation_params = entry.GetIfDict();
    if (!invocation_params) {
      ReportConfigurationError(kBadParams);
      return;
    }

    // Max length of the registry and histogram suffix. Fairly arbitrary: the
    // Windows registry accepts much longer keys, but we need to display this
    // string in histograms as well.
    constexpr size_t kMaxSuffixLength = 80;

    // The suffix must be an alphanumeric string. (Empty is fine as long as the
    // "suffix" key is present.)
    const std::string* suffix = invocation_params->FindString("suffix");
    if (!suffix || !ValidateString(*suffix, std::string(), kMaxSuffixLength)) {
      ReportConfigurationError(kBadParams);
      return;
    }

    // Build a command line for the reporter out of the executable path and the
    // arguments from the manifest. (The "arguments" key must be present, but
    // it's ok if it's an empty list or a list of empty strings.)
    const base::Value::List* arguments =
        invocation_params->FindList("arguments");
    if (!arguments) {
      ReportConfigurationError(kBadParams);
      return;
    }

    std::vector<std::wstring> argv = {exe_path.value()};
    for (const base::Value& value : *arguments) {
      const std::string* argument = value.GetIfString();
      if (!argument) {
        ReportConfigurationError(kBadParams);
        return;
      }
      if (!argument->empty())
        argv.push_back(base::UTF8ToWide(*argument));
    }

    base::CommandLine command_line(argv);
    command_line.AppendSwitchASCII(chrome_cleaner::kSessionIdSwitch,
                                   session_id);

    // Add the histogram suffix to the command-line as well, so that the
    // reporter will add the same suffix to registry keys where it writes
    // metrics.
    if (!suffix->empty()) {
      command_line.AppendSwitchASCII(chrome_cleaner::kRegistrySuffixSwitch,
                                     *suffix);
    }

    SwReporterInvocation::Behaviours supported_behaviours = 0;
    if (!GetOptionalBehaviour(*invocation_params, "prompt",
                              SwReporterInvocation::BEHAVIOUR_TRIGGER_PROMPT,
                              &supported_behaviours)) {
      return;
    }

    invocations.PushInvocation(
        SwReporterInvocation(command_line)
            .WithSuffix(*suffix)
            .WithSupportedBehaviours(supported_behaviours));
  }

  ready_callback.Run(*prompt_seed, std::move(invocations));
}

void ReportOnDemandUpdateSucceededHistogram(bool value) {
  UMA_HISTOGRAM_BOOLEAN("SoftwareReporter.OnDemandUpdateSucceeded", value);
}

}  // namespace

SwReporterInstallerPolicy::SwReporterInstallerPolicy(
    PrefService* prefs,
    OnComponentReadyCallback on_component_ready_callback)
    : prefs_(prefs),
      on_component_ready_callback_(on_component_ready_callback) {}

SwReporterInstallerPolicy::~SwReporterInstallerPolicy() = default;

void SwReporterInstallerPolicy::SetRandomReporterCohortForTesting(
    const std::string& cohort_name) {
  random_cohort_for_testing_ = cohort_name;
}

bool SwReporterInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& dir) const {
  return base::PathExists(dir.Append(kSwReporterExeName));
}

bool SwReporterInstallerPolicy::SupportsGroupPolicyEnabledComponentUpdates()
    const {
  return true;
}

bool SwReporterInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result SwReporterInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);
}

void SwReporterInstallerPolicy::OnCustomUninstall() {}

void SwReporterInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  ScheduleSoftwareReporterWithManifest(
      install_dir.Append(kSwReporterExeName), version, std::move(manifest),
      // Unless otherwise specified by a unit test, This will post
      // |safe_browsing::OnSwReporterReady| to the UI thread.
      on_component_ready_callback_);
}

base::FilePath SwReporterInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("SwReporter"));
}

void SwReporterInstallerPolicy::GetHash(std::vector<uint8_t>* hash) const {
  DCHECK(hash);
  hash->assign(kSwReporterSha2Hash,
               kSwReporterSha2Hash + sizeof(kSwReporterSha2Hash));
}

std::string SwReporterInstallerPolicy::GetName() const {
  return "Software Reporter Tool";
}

update_client::InstallerAttributes
SwReporterInstallerPolicy::GetInstallerAttributes() const {
  update_client::InstallerAttributes attributes;
  // Pass the tag parameter to the installer as the "tag" attribute; it will be
  // used to choose which binary is downloaded.
  attributes["tag"] = GetReporterCohortTag(prefs_);
  return attributes;
}

// Returns the reporter cohort tag by checking the feature list, then `prefs`,
// then assigning the tag randomly if it's not found in either.
std::string SwReporterInstallerPolicy::GetReporterCohortTag(
    PrefService* prefs) const {
  const std::string feature_tag =
      safe_browsing::kReporterDistributionTagParam.Get();
  if (!feature_tag.empty()) {
    // If the tag is not a valid attribute (see the regexp in
    // ComponentInstallerPolicy::InstallerAttributes), set it to a valid but
    // unrecognized value so that nothing will be downloaded.
    constexpr size_t kMaxAttributeLength = 256;
    constexpr char kExtraAttributeChars[] = "-.,;+_=";
    if (!ValidateString(feature_tag, kExtraAttributeChars,
                        kMaxAttributeLength)) {
      ReportConfigurationError(kBadTag);
      return "missing_tag";
    }

    // Any tag that doesn't contain invalid characters is valid, so that the
    // feature can be used to experiment with new versions.
    return feature_tag;
  }

  // Use the tag from preferences. Only "canary" and "stable" are valid, so
  // that bad values in local prefs don't block access to the reporter. Note
  // there's no need to clear invalid tags since they'll be overwritten by the
  // new cohort values.
  const std::string prefs_tag = prefs->GetString(prefs::kSwReporterCohort);
  if (prefs_tag == "canary" || prefs_tag == "stable") {
    // Re-randomize unless the cohort was assigned less than a month ago. Also
    // ignore invalid selection times that are too far in the future.
    const base::Time last_selection_time =
        prefs->GetTime(prefs::kSwReporterCohortSelectionTime);
    const base::Time now = base::Time::Now();
    if (now - base::Days(30) < last_selection_time &&
        last_selection_time < now + base::Days(1)) {
      return prefs_tag;
    }
  }
  // Chrome Stable users have a 95% chance to get the stable reporter, 5% chance
  // to get the canary reporter. All other Chrome channels are assigned 50/50.
  const double stable_reporter_probability =
      (chrome::GetChannel() == version_info::Channel::STABLE) ? 0.95 : 0.5;
  const std::string selected_tag =
      !random_cohort_for_testing_.empty()
          ? random_cohort_for_testing_
          : ((base::RandDouble() < stable_reporter_probability) ? "stable"
                                                                : "canary");
  prefs->SetString(prefs::kSwReporterCohort, selected_tag);
  prefs->SetTime(prefs::kSwReporterCohortSelectionTime, base::Time::Now());
  return selected_tag;
}

SwReporterOnDemandFetcher::SwReporterOnDemandFetcher(
    ComponentUpdateService* cus,
    base::OnceClosure on_error_callback)
    : cus_(cus), on_error_callback_(std::move(on_error_callback)) {
  cus_->AddObserver(this);
  cus_->GetOnDemandUpdater().OnDemandUpdate(
      kSwReporterComponentId, OnDemandUpdater::Priority::FOREGROUND,
      Callback());
}

SwReporterOnDemandFetcher::~SwReporterOnDemandFetcher() {
  cus_->RemoveObserver(this);
}

void SwReporterOnDemandFetcher::OnEvent(Events event, const std::string& id) {
  if (id != kSwReporterComponentId)
    return;

  if (event == Events::COMPONENT_ALREADY_UP_TO_DATE ||
      event == Events::COMPONENT_UPDATE_ERROR) {
    ReportOnDemandUpdateSucceededHistogram(false);
    std::move(on_error_callback_).Run();
    cus_->RemoveObserver(this);
  } else if (event == Events::COMPONENT_UPDATED) {
    ReportOnDemandUpdateSucceededHistogram(true);
    cus_->RemoveObserver(this);
  }
}

void RegisterSwReporterComponent(ComponentUpdateService* cus,
                                 PrefService* prefs) {
  base::ScopedClosureRunner runner(std::move(GetRegistrationCBForTesting()));

  // Don't install the component if not allowed by policy.  This prevents
  // downloads and background scans.
  if (!is_sw_reporter_enabled || !safe_browsing::SwReporterIsAllowedByPolicy())
    return;

  ReportUMAForLastCleanerRun();

  // Once the component is ready and browser startup is complete, run
  // |safe_browsing::OnSwReporterReady|.
  using ChromeCleanerController = safe_browsing::ChromeCleanerController;
  OnComponentReadyCallback ready_callback = base::BindRepeating(
      [](const std::string& prompt_seed,
         safe_browsing::SwReporterInvocationSequence&& invocations) {
        // Unretained is safe since ChromeCleanerController is a leaked global
        // singleton.
        content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
            ->PostTask(
                FROM_HERE,
                base::BindOnce(
                    &ChromeCleanerController::OnSwReporterReady,
                    base::Unretained(ChromeCleanerController::GetInstance()),
                    prompt_seed, std::move(invocations)));
      });

  // Install the component.
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<SwReporterInstallerPolicy>(prefs,
                                                  std::move(ready_callback)));

  installer->Register(cus, runner.Release());
}

void SetRegisterSwReporterComponentCallbackForTesting(
    base::OnceClosure registration_cb) {
  is_sw_reporter_enabled = true;
  GetRegistrationCBForTesting() = std::move(registration_cb);
}

void RegisterPrefsForSwReporter(PrefRegistrySimple* registry) {
  // The two "LastTime" prefs are Int64 instead of Time for legacy reasons.
  // Changing the format would need an upgrade path, which is more complicated
  // than it's worth.
  registry->RegisterInt64Pref(prefs::kSwReporterLastTimeTriggered, 0);
  registry->RegisterIntegerPref(prefs::kSwReporterLastExitCode, -1);
  registry->RegisterInt64Pref(prefs::kSwReporterLastTimeSentReport, 0);
  registry->RegisterBooleanPref(prefs::kSwReporterEnabled, true);
  registry->RegisterStringPref(prefs::kSwReporterCohort, "");
  registry->RegisterTimePref(prefs::kSwReporterCohortSelectionTime,
                             base::Time());
}

void RegisterProfilePrefsForSwReporter(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kSwReporterPromptVersion, "");
  registry->RegisterStringPref(prefs::kSwReporterPromptSeed, "");
  registry->RegisterBooleanPref(prefs::kSwReporterReportingEnabled, true);
}

void ReportUMAForLastCleanerRun() {
  std::wstring cleaner_key_name =
      chrome_cleaner::kSoftwareRemovalToolRegistryKey;
  cleaner_key_name.append(1, L'\\').append(chrome_cleaner::kCleanerSubKey);
  base::win::RegKey cleaner_key(HKEY_CURRENT_USER, cleaner_key_name.c_str(),
                                KEY_ALL_ACCESS);
  // Cleaner is assumed to have run if we have a start time.
  if (cleaner_key.Valid()) {
    if (cleaner_key.HasValue(chrome_cleaner::kStartTimeValueName)) {
      // Get start & end time. If we don't have an end time, we can assume the
      // cleaner has not completed.
      int64_t start_time_value = {};
      cleaner_key.ReadInt64(chrome_cleaner::kStartTimeValueName,
                            &start_time_value);
      const base::Time start_time = base::Time::FromDeltaSinceWindowsEpoch(
          base::Microseconds(start_time_value));

      const bool completed =
          cleaner_key.HasValue(chrome_cleaner::kEndTimeValueName);
      if (completed) {
        int64_t end_time_value = {};
        cleaner_key.ReadInt64(chrome_cleaner::kEndTimeValueName,
                              &end_time_value);
        const base::Time end_time = base::Time::FromDeltaSinceWindowsEpoch(
            base::Microseconds(end_time_value));

        cleaner_key.DeleteValue(chrome_cleaner::kEndTimeValueName);
        UMA_HISTOGRAM_LONG_TIMES("SoftwareReporter.Cleaner.RunningTime",
                                 end_time - start_time);
      }
      // Get exit code. Assume nothing was found if we can't read the exit code.
      DWORD exit_code = chrome_cleaner::kSwReporterNothingFound;
      if (cleaner_key.HasValue(chrome_cleaner::kExitCodeValueName)) {
        cleaner_key.ReadValueDW(chrome_cleaner::kExitCodeValueName, &exit_code);
        base::UmaHistogramSparse("SoftwareReporter.Cleaner.ExitCode",
                                 exit_code);
        cleaner_key.DeleteValue(chrome_cleaner::kExitCodeValueName);
      }
      cleaner_key.DeleteValue(chrome_cleaner::kStartTimeValueName);

      if (exit_code == chrome_cleaner::kSwReporterPostRebootCleanupNeeded ||
          exit_code ==
              chrome_cleaner::kSwReporterDelayedPostRebootCleanupNeeded) {
        // Check if we are running after the user has rebooted.
        const base::TimeDelta elapsed = base::Time::Now() - start_time;
        DCHECK_GT(elapsed.InMilliseconds(), 0);
      }

      if (cleaner_key.HasValue(chrome_cleaner::kUploadResultsValueName)) {
        std::wstring upload_results;
        cleaner_key.ReadValue(chrome_cleaner::kUploadResultsValueName,
                              &upload_results);
        ReportUploadsWithUma(base::WideToUTF16(upload_results));
      }
    } else {
      if (cleaner_key.HasValue(chrome_cleaner::kEndTimeValueName)) {
        cleaner_key.DeleteValue(chrome_cleaner::kEndTimeValueName);
      }
    }
  }
}

}  // namespace component_updater
