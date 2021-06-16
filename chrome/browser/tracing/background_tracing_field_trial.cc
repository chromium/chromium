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
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/tracing/crash_service_uploader.h"
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

void OnBackgroundTracingUploadComplete(
    TraceCrashServiceUploader* uploader,
    BackgroundTracingManager::FinishedProcessingCallback done_callback,
    bool success,
    const std::string& feedback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(done_callback).Run(success);
}

void BackgroundTracingUploadCallback(
    const std::string& upload_url,
    std::unique_ptr<std::string> file_contents,
    BackgroundTracingManager::FinishedProcessingCallback callback) {
  TraceCrashServiceUploader* uploader = new TraceCrashServiceUploader(
      g_browser_process->shared_url_loader_factory());

  if (GURL(upload_url).is_valid())
    uploader->SetUploadURL(upload_url);

#if defined(OS_ANDROID)
  auto connection_type = net::NetworkChangeNotifier::GetConnectionType();
  if (connection_type != net::NetworkChangeNotifier::CONNECTION_WIFI &&
      connection_type != net::NetworkChangeNotifier::CONNECTION_ETHERNET &&
      connection_type != net::NetworkChangeNotifier::CONNECTION_BLUETOOTH) {
    // Allow only 100KiB for uploads over data.
    uploader->SetMaxUploadBytes(100 * 1024);
  }
#endif
  std::unique_ptr<base::DictionaryValue> metadata =
      TraceEventMetadataSource::GetInstance()->GenerateLegacyMetadataDict();

  uploader->DoUpload(
      *file_contents, content::TraceUploader::UNCOMPRESSED_UPLOAD,
      std::move(metadata), content::TraceUploader::UploadProgressCallback(),
      base::BindOnce(&OnBackgroundTracingUploadComplete, base::Owned(uploader),
                     std::move(callback)));
}

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
                                          const base::FilePath& output_file,
                                          const std::string& upload_url) {
  // Exactly one destination should be given.
  BackgroundTracingManager::ReceiveCallback receive_callback;
  if (!output_file.empty()) {
    DCHECK(upload_url.empty());
    receive_callback = base::BindRepeating(&WriteTraceToFile, output_file);
  } else if (!upload_url.empty()) {
    DCHECK(output_file.empty());
    receive_callback =
        base::BindRepeating(&BackgroundTracingUploadCallback, upload_url);
  } else {
    NOTREACHED();
    return;
  }

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

  const base::DictionaryValue* dict = nullptr;
  if (!value_with_error.value->GetAsDictionary(&dict)) {
    LOG(ERROR) << "Background tracing config is not a dict";
    return;
  }

  std::unique_ptr<BackgroundTracingConfig> config =
      BackgroundTracingConfig::FromDict(dict);
  if (!config) {
    LOG(ERROR) << "Background tracing config dict has invalid contents";
    return;
  }

  // Consider all tracing set up with a local config file to have local output
  // for metrics, since even if `upload_url` is given it will point to a local
  // test server and not a production upload endpoint.
  BackgroundTracingManager::GetInstance()->SetActiveScenarioWithReceiveCallback(
      std::move(config), std::move(receive_callback),
      BackgroundTracingManager::NO_DATA_FILTERING,
      /*local_output=*/true);
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
  const auto upload_url =
      command_line->GetSwitchValueNative(switches::kTraceUploadURL);
  if ((output_file.empty() && upload_url.empty()) ||
      (!output_file.empty() && !upload_url.empty())) {
    LOG(ERROR) << "Specify one of --background-tracing-output-file or "
                  "--trace-upload-url";
    return BackgroundTracingSetupMode::kDisabledInvalidCommandLine;
  }

  if (!upload_url.empty() &&
      base::FeatureList::IsEnabled(features::kBackgroundTracingProtoOutput)) {
    LOG(ERROR) << "--trace-upload-url can only be used with legacy JSON traces";
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
              switches::kBackgroundTracingOutputFile),
          command_line->GetSwitchValueASCII(switches::kTraceUploadURL));
      return;
    }
    case BackgroundTracingSetupMode::kFromFieldTrial:
      // Fall through.
      break;
  }

  auto* manager = BackgroundTracingManager::GetInstance();
  std::unique_ptr<BackgroundTracingConfig> config =
      manager->GetBackgroundTracingConfig(kBackgroundTracingFieldTrial);

  if (base::FeatureList::IsEnabled(features::kBackgroundTracingProtoOutput)) {
    manager->SetActiveScenario(std::move(config),
                               BackgroundTracingManager::ANONYMIZE_DATA);
  } else {
    // JSON traces must be uploaded through BackgroundTracingUploadCallback.
    manager->SetActiveScenarioWithReceiveCallback(
        std::move(config),
        base::BindRepeating(&BackgroundTracingUploadCallback,
                            manager->GetBackgroundTracingUploadUrl(
                                kBackgroundTracingFieldTrial)),
        BackgroundTracingManager::ANONYMIZE_DATA,
        /*local_output=*/false);
  }
}

}  // namespace tracing
