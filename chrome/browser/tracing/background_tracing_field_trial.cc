// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/background_tracing_field_trial.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/metrics/field_trial.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_log.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "url/gurl.h"

namespace tracing {

namespace {

using content::BackgroundTracingConfig;
using content::BackgroundTracingManager;

const char kBackgroundTracingFieldTrial[] = "BackgroundTracing";

bool BlockingWriteTraceToFile(const base::FilePath& output_file,
                              std::unique_ptr<std::string> file_contents) {
  if (base::WriteFile(output_file, *file_contents)) {
    LOG(ERROR) << "Background trace written to "
               << output_file.LossyDisplayName();
    return true;
  }
  LOG(ERROR) << "Failed to write background trace to "
             << output_file.LossyDisplayName();
  return false;
}

void WriteTraceToFile(
    const base::FilePath& output_file,
    std::unique_ptr<std::string> file_contents,
    BackgroundTracingManager::FinishedProcessingCallback done_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&BlockingWriteTraceToFile, output_file,
                     std::move(file_contents)),
      std::move(done_callback));
}

void SetupBackgroundTracingFromConfigFile(const base::FilePath& config_file,
                                          const base::FilePath& output_file) {
  BackgroundTracingManager::ReceiveCallback receive_callback;
  DCHECK(!output_file.empty()) << "Output file for trace must be specified.";

  receive_callback = base::BindRepeating(&WriteTraceToFile, output_file);

  std::string config_text;
  if (!base::ReadFileToString(config_file, &config_text) ||
      config_text.empty()) {
    LOG(ERROR) << "Failed to read background tracing config file "
               << config_file.value();
    return;
  }

  base::JSONReader::ValueWithError value_with_error =
      base::JSONReader::ReadAndReturnValueWithError(
          config_text, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!value_with_error.value) {
    LOG(ERROR) << "Background tracing has incorrect config: "
               << value_with_error.error_message;
    return;
  }

  if (!value_with_error.value->is_dict()) {
    LOG(ERROR) << "Background tracing config is not a dict";
    return;
  }

  std::unique_ptr<BackgroundTracingConfig> config =
      BackgroundTracingConfig::FromDict(std::move(*(value_with_error.value)));
  if (!config) {
    LOG(ERROR) << "Background tracing config dict has invalid contents";
    return;
  }

  // Consider all tracing set up with a local config file to have local output
  // for metrics, since even if `upload_url` is given it will point to a local
  // test server and not a production upload endpoint.
  BackgroundTracingManager::GetInstance()->SetActiveScenarioWithReceiveCallback(
      std::move(config), std::move(receive_callback),
      BackgroundTracingManager::NO_DATA_FILTERING);
}

}  // namespace

BackgroundTracingSetupMode GetBackgroundTracingSetupMode() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kEnableBackgroundTracing))
    return BackgroundTracingSetupMode::kFromFieldTrial;

  if (command_line->GetSwitchValueNative(switches::kEnableBackgroundTracing)
          .empty()) {
    LOG(ERROR) << "--enable-background-tracing needs a config file path";
    return BackgroundTracingSetupMode::kDisabledInvalidCommandLine;
  }

  const auto output_file = command_line->GetSwitchValueNative(
      switches::kBackgroundTracingOutputFile);
  if (output_file.empty()) {
    LOG(ERROR) << "Specify --background-tracing-output-file";
    return BackgroundTracingSetupMode::kDisabledInvalidCommandLine;
  }

  return BackgroundTracingSetupMode::kFromConfigFile;
}

void SetupBackgroundTracingFieldTrial() {
  switch (GetBackgroundTracingSetupMode()) {
    case BackgroundTracingSetupMode::kDisabledInvalidCommandLine:
      // Abort setup.
      return;
    case BackgroundTracingSetupMode::kFromConfigFile: {
      auto* command_line = base::CommandLine::ForCurrentProcess();
      SetupBackgroundTracingFromConfigFile(
          command_line->GetSwitchValuePath(switches::kEnableBackgroundTracing),
          command_line->GetSwitchValuePath(
              switches::kBackgroundTracingOutputFile));
      return;
    }
    case BackgroundTracingSetupMode::kFromFieldTrial:
      // Fall through.
      break;
  }

  auto* manager = BackgroundTracingManager::GetInstance();
  std::unique_ptr<BackgroundTracingConfig> config =
      manager->GetBackgroundTracingConfig(kBackgroundTracingFieldTrial);

  manager->SetActiveScenario(std::move(config),
                             BackgroundTracingManager::ANONYMIZE_DATA);
}

}  // namespace tracing
