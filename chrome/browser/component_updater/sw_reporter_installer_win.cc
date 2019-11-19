// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/sw_reporter_installer_win.h"

#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/reporter_runner_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/update_client/update_client.h"
#include "components/update_client/utils.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace component_updater {

namespace {

using safe_browsing::SwReporterInvocation;
using safe_browsing::SwReporterInvocationSequence;

// These values are used to send UMA information and are replicated in the
// histograms.xml file, so the order MUST NOT CHANGE.
enum SRTCompleted {
  SRT_COMPLETED_NOT_YET = 0,
  SRT_COMPLETED_YES = 1,
  SRT_COMPLETED_LATER = 2,
  SRT_COMPLETED_MAX,
};

// CRX hash. The extension id is: gkmgaooipdjhmangpemjhigmamcehddo. The hash was
// generated in Python with something like this:
// hashlib.sha256().update(open("<file>.crx").read()[16:16+294]).digest().
const uint8_t kSwReporterSha2Hash[] = {
    0x6a, 0xc6, 0x0e, 0xe8, 0xf3, 0x97, 0xc0, 0xd6, 0xf4, 0xc9, 0x78,
    0x6c, 0x0c, 0x24, 0x73, 0x3e, 0x05, 0xa5, 0x62, 0x4b, 0x2e, 0xc7,
    0xb7, 0x1c, 0x5f, 0xea, 0xf0, 0x88, 0xf6, 0x97, 0x9b, 0xc7};

const base::FilePath::CharType kSwReporterExeName[] =
    FILE_PATH_LITERAL("software_reporter_tool.exe");

// SwReporter is normally only registered in official builds.  However, to
// enable testing in chromium build bots, test code can set this to true.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
bool is_sw_reporter_enabled = true;
#else
bool is_sw_reporter_enabled = false;
#endif

// Callback function to be called once the registration of the component
// is complete.  This is used only in tests.
base::OnceClosure* registration_cb_for_testing = new base::OnceClosure();

void SRTHasCompleted(SRTCompleted value) {
  UMA_HISTOGRAM_ENUMERATION("SoftwareReporter.Cleaner.HasCompleted", value,
                            SRT_COMPLETED_MAX);
}

void ReportUploadsWithUma(const base::string16& upload_results) {
  base::String16Tokenizer tokenizer(upload_results, STRING16_LITERAL(";"));
  int failure_count = 0;
  int success_count = 0;
  int longest_failure_run = 0;
  int current_failure_run = 0;
  bool last_result = false;
  while (tokenizer.GetNext()) {
    if (tokenizer.token_piece() == STRING16_LITERAL("0")) {
      ++failure_count;
      ++current_failure_run;
      last_result = false;
    } else {
      ++success_count;
      current_failure_run = 0;
      last_result = true;
    }

    if (current_failure_run > longest_failure_run)
      longest_failure_run = current_failure_run;
  }

  UMA_HISTOGRAM_COUNTS_100("SoftwareReporter.UploadFailureCount",
                           failure_count);
  UMA_HISTOGRAM_COUNTS_100("SoftwareReporter.UploadSuccessCount",
                           success_count);
  UMA_HISTOGRAM_COUNTS_100("SoftwareReporter.UploadLongestFailureRun",
                           longest_failure_run);
  UMA_HISTOGRAM_BOOLEAN("SoftwareReporter.LastUploadResult", last_result);
}

void ReportExperimentError(SoftwareReporterExperimentError error) {
  UMA_HISTOGRAM_ENUMERATION("SoftwareReporter.ExperimentErrors", error,
                            SW_REPORTER_EXPERIMENT_ERROR_MAX);
}

// Ensures |str| contains only alphanumeric characters and characters from
// |extras|, and is not longer than |max_length|.
bool ValidateString(const std::string& str,
                    const std::string& extras,
                    size_t max_length) {
  return str.size() <= max_length &&
         std::all_of(str.cbegin(), str.cend(), [&extras](char c) {
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
    const base::DictionaryValue* invocation_params,
    base::StringPiece behaviour_name,
    SwReporterInvocation::Behaviours behaviour_flag,
    SwReporterInvocation::Behaviours* supported_behaviours) {
  DCHECK(invocation_params);
  DCHECK(supported_behaviours);

  // Parameters enabling behaviours are optional, but if present must be
  // boolean.
  const base::Value* value = nullptr;
  if (invocation_params->Get(behaviour_name, &value)) {
    bool enable_behaviour = false;
    if (!value->GetAsBoolean(&enable_behaviour)) {
      ReportExperimentError(SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS);
      return false;
    }
    if (enable_behaviour)
      *supported_behaviours |= behaviour_flag;
  }
  return true;
}

// Reads the command-line params and an UMA histogram suffix from the manifest
// and adds the invocations to be run to |out_sequence|.
// Returns whether the manifest was successfully read.
bool ExtractInvocationSequenceFromManifest(
    const base::FilePath& exe_path,
    std::unique_ptr<base::DictionaryValue> manifest,
    safe_browsing::SwReporterInvocationSequence* out_sequence) {
  const base::ListValue* parameter_list = nullptr;

  // Allow an empty or missing launch_params list, but log an error if
  // launch_params cannot be parsed as a list.
  base::Value* launch_params = nullptr;
  if (manifest->Get("launch_params", &launch_params) &&
      !launch_params->GetAsList(&parameter_list)) {
    ReportExperimentError(SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS);
    return false;
  }

  // Use a random session id to link reporter invocations together.
  const std::string session_id = GenerateSessionId();

  // If there are no launch parameters, create a single invocation with default
  // behaviour.
  if (!parameter_list || parameter_list->empty()) {
    base::CommandLine command_line(exe_path);
    command_line.AppendSwitchASCII(chrome_cleaner::kSessionIdSwitch,
                                   session_id);
    out_sequence->PushInvocation(
        SwReporterInvocation(command_line)
            .WithSupportedBehaviours(
                SwReporterInvocation::BEHAVIOURS_ENABLED_BY_DEFAULT));
    return true;
  }

  for (const auto& iter : *parameter_list) {
    const base::DictionaryValue* invocation_params = nullptr;
    if (!iter.GetAsDictionary(&invocation_params)) {
      ReportExperimentError(SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS);
      return false;
    }

    // Max length of the registry and histogram suffix. Fairly arbitrary: the
    // Windows registry accepts much longer keys, but we need to display this
    // string in histograms as well.
    constexpr size_t kMaxSuffixLength = 80;

    // The suffix must be an alphanumeric string. (Empty is fine as long as the
    // "suffix" key is present.)
    std::string suffix;
    if (!invocation_params->GetString("suffix", &suffix) ||
        !ValidateString(suffix, std::string(), kMaxSuffixLength)) {
      ReportExperimentError(SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS);
      return false;
    }

    // Build a command line for the reporter out of the executable path and the
    // arguments from the manifest. (The "arguments" key must be present, but
    // it's ok if it's an empty list or a list of empty strings.)
    const base::ListValue* arguments = nullptr;
    if (!invocation_params->GetList("arguments", &arguments)) {
      ReportExperimentError(SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS);
      return false;
    }

    std::vector<base::string16> argv = {exe_path.value()};
    for (const auto& value : *arguments) {
      base::string16 argument;
      if (!value.GetAsString(&argument)) {
        ReportExperimentError(SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS);
        return false;
      }
      if (!argument.empty())
        argv.push_back(argument);
    }

    base::CommandLine command_line(argv);
    command_line.AppendSwitchASCII(chrome_cleaner::kSessionIdSwitch,
                                   session_id);

    // Add the histogram suffix to the command-line as well, so that the
    // reporter will add the same suffix to registry keys where it writes
    // metrics.
    if (!suffix.empty())
      command_line.AppendSwitchASCII(chrome_cleaner::kRegistrySuffixSwitch,
                                     suffix);

    SwReporterInvocation::Behaviours supported_behaviours = 0;
    if (!GetOptionalBehaviour(invocation_params, "prompt",
                              SwReporterInvocation::BEHAVIOUR_TRIGGER_PROMPT,
                              &supported_behaviours)) {
      return false;
    }

    out_sequence->PushInvocation(
        SwReporterInvocation(command_line)
            .WithSuffix(suffix)
            .WithSupportedBehaviours(supported_behaviours));
  }

  return true;
}

void ReportOnDemandUpdateSucceededHistogram(bool value) {
  UMA_HISTOGRAM_BOOLEAN("SoftwareReporter.OnDemandUpdateSucceeded", value);
}

}  // namespace

SwReporterInstallerPolicy::SwReporterInstallerPolicy(
    const OnComponentReadyCallback& on_component_ready_callback)
    : on_component_ready_callback_(on_component_ready_callback) {}

SwReporterInstallerPolicy::~SwReporterInstallerPolicy() = default;

bool SwReporterInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
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
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);
}

void SwReporterInstallerPolicy::OnCustomUninstall() {}

void SwReporterInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  safe_browsing::SwReporterInvocationSequence invocations(version);
  const base::FilePath exe_path(install_dir.Append(kSwReporterExeName));
  if (ExtractInvocationSequenceFromManifest(exe_path, std::move(manifest),
                                            &invocations)) {
    // Unless otherwise specified by a unit test, This will post
    // |safe_browsing::OnSwReporterReady| to the UI thread.
    on_component_ready_callback_.Run(std::move(invocations));
  }
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
  if (base::FeatureList::IsEnabled(
          safe_browsing::kChromeCleanupDistributionFeature)) {
    // Pass the tag parameter to the installer as the "tag" attribute; it will
    // be used to choose which binary is downloaded.
    constexpr char kTagParamName[] = "reporter_omaha_tag";
    const std::string tag = variations::GetVariationParamValueByFeature(
        safe_browsing::kChromeCleanupDistributionFeature, kTagParamName);

    // If the tag is not a valid attribute (see the regexp in
    // ComponentInstallerPolicy::InstallerAttributes), set it to a valid but
    // unrecognized value so that nothing will be downloaded.
    constexpr size_t kMaxAttributeLength = 256;
    constexpr char kExtraAttributeChars[] = "-.,;+_=";
    constexpr char kTagParam[] = "tag";
    if (tag.empty() ||
        !ValidateString(tag, kExtraAttributeChars, kMaxAttributeLength)) {
      ReportExperimentError(SW_REPORTER_EXPERIMENT_ERROR_BAD_TAG);
      attributes[kTagParam] = "missing_tag";
    } else {
      attributes[kTagParam] = tag;
    }
  }
  return attributes;
}

std::vector<std::string> SwReporterInstallerPolicy::GetMimeTypes() const {
  return std::vector<std::string>();
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

  if (event == Events::COMPONENT_NOT_UPDATED ||
      event == Events::COMPONENT_UPDATE_ERROR) {
    ReportOnDemandUpdateSucceededHistogram(false);
    std::move(on_error_callback_).Run();
    cus_->RemoveObserver(this);
  } else if (event == Events::COMPONENT_UPDATED) {
    ReportOnDemandUpdateSucceededHistogram(true);
    cus_->RemoveObserver(this);
  }
}

void RegisterSwReporterComponent(ComponentUpdateService* cus) {
  base::ScopedClosureRunner runner(std::move(*registration_cb_for_testing));

  // Don't install the component if not allowed by policy.  This prevents
  // downloads and background scans.
  if (!is_sw_reporter_enabled || !safe_browsing::SwReporterIsAllowedByPolicy())
    return;

  ReportUMAForLastCleanerRun();

  // Once the component is ready and browser startup is complete, run
  // |safe_browsing::OnSwReporterReady|.
  auto lambda = [](safe_browsing::SwReporterInvocationSequence&& invocations) {
    base::PostTask(
        FROM_HERE,
        {content::BrowserThread::UI, base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            &safe_browsing::ChromeCleanerController::OnSwReporterReady,
            base::Unretained(
                safe_browsing::ChromeCleanerController::GetInstance()),
            base::Passed(&invocations)));
  };

  // Install the component.
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<SwReporterInstallerPolicy>(base::BindRepeating(lambda)));
  installer->Register(cus, runner.Release());
}

void SetRegisterSwReporterComponentCallbackForTesting(
    base::OnceClosure registration_cb) {
  is_sw_reporter_enabled = true;
  *registration_cb_for_testing = std::move(registration_cb);
}

void RegisterPrefsForSwReporter(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kSwReporterLastTimeTriggered, 0);
  registry->RegisterIntegerPref(prefs::kSwReporterLastExitCode, -1);
  registry->RegisterInt64Pref(prefs::kSwReporterLastTimeSentReport, 0);
  registry->RegisterBooleanPref(prefs::kSwReporterEnabled, true);
}

void RegisterProfilePrefsForSwReporter(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kSwReporterPromptVersion, "");
  registry->RegisterStringPref(prefs::kSwReporterPromptSeed, "");
  registry->RegisterBooleanPref(prefs::kSwReporterReportingEnabled, true);
}

void ReportUMAForLastCleanerRun() {
  base::string16 cleaner_key_name =
      chrome_cleaner::kSoftwareRemovalToolRegistryKey;
  cleaner_key_name.append(1, L'\\').append(chrome_cleaner::kCleanerSubKey);
  base::win::RegKey cleaner_key(HKEY_CURRENT_USER, cleaner_key_name.c_str(),
                                KEY_ALL_ACCESS);
  // Cleaner is assumed to have run if we have a start time.
  if (cleaner_key.Valid()) {
    if (cleaner_key.HasValue(chrome_cleaner::kStartTimeValueName)) {
      // Get version number.
      if (cleaner_key.HasValue(chrome_cleaner::kVersionValueName)) {
        DWORD version = {};
        cleaner_key.ReadValueDW(chrome_cleaner::kVersionValueName, &version);
        base::UmaHistogramSparse("SoftwareReporter.Cleaner.Version", version);
        cleaner_key.DeleteValue(chrome_cleaner::kVersionValueName);
      }
      // Get start & end time. If we don't have an end time, we can assume the
      // cleaner has not completed.
      int64_t start_time_value = {};
      cleaner_key.ReadInt64(chrome_cleaner::kStartTimeValueName,
                            &start_time_value);
      const base::Time start_time = base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromMicroseconds(start_time_value));

      const bool completed =
          cleaner_key.HasValue(chrome_cleaner::kEndTimeValueName);
      SRTHasCompleted(completed ? SRT_COMPLETED_YES : SRT_COMPLETED_NOT_YET);
      if (completed) {
        int64_t end_time_value = {};
        cleaner_key.ReadInt64(chrome_cleaner::kEndTimeValueName,
                              &end_time_value);
        const base::Time end_time = base::Time::FromDeltaSinceWindowsEpoch(
            base::TimeDelta::FromMicroseconds(end_time_value));

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
        UMA_HISTOGRAM_BOOLEAN(
            "SoftwareReporter.Cleaner.HasRebooted",
            static_cast<uint64_t>(elapsed.InMilliseconds()) > ::GetTickCount());
      }

      if (cleaner_key.HasValue(chrome_cleaner::kUploadResultsValueName)) {
        base::string16 upload_results;
        cleaner_key.ReadValue(chrome_cleaner::kUploadResultsValueName,
                              &upload_results);
        ReportUploadsWithUma(upload_results);
      }
    } else {
      if (cleaner_key.HasValue(chrome_cleaner::kEndTimeValueName)) {
        SRTHasCompleted(SRT_COMPLETED_LATER);
        cleaner_key.DeleteValue(chrome_cleaner::kEndTimeValueName);
      }
    }
  }
}

}  // namespace component_updater
