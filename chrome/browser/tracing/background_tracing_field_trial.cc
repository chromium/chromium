// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/background_tracing_field_trial.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/metrics/field_trial.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/tracing/crash_service_uploader.h"
#include "chrome/common/chrome_switches.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "url/gurl.h"

namespace tracing {

namespace {

const char kBackgroundTracingFieldTrial[] = "BackgroundTracing";
const char kBackgroundTracingConfig[] = "config";
const char kBackgroundTracingUploadUrl[] = "upload_url";

ConfigTextFilterForTesting g_config_text_filter_for_testing = nullptr;

void OnBackgroundTracingUploadComplete(
    TraceCrashServiceUploader* uploader,
    content::BackgroundTracingManager::FinishedProcessingCallback done_callback,
    bool success,
    const std::string& feedback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(done_callback).Run(success);
}

void BackgroundTracingUploadCallback(
    const std::string& upload_url,
    std::unique_ptr<std::string> file_contents,
    content::BackgroundTracingManager::FinishedProcessingCallback callback) {
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

std::unique_ptr<content::BackgroundTracingConfig> GetBackgroundTracingConfig() {
  std::string config_text = variations::GetVariationParamValue(
      kBackgroundTracingFieldTrial, kBackgroundTracingConfig);

  if (config_text.empty())
    return nullptr;

  if (g_config_text_filter_for_testing)
    (*g_config_text_filter_for_testing)(&config_text);

  std::unique_ptr<base::Value> value =
      base::JSONReader::ReadDeprecated(config_text);
  if (!value)
    return nullptr;

  const base::DictionaryValue* dict = nullptr;
  if (!value->GetAsDictionary(&dict))
    return nullptr;

  return content::BackgroundTracingConfig::FromDict(dict);
}

void SetupBackgroundTracingFromConfigFile(const base::FilePath& config_file,
                                          const std::string& upload_url) {
  std::string config_text;
  if (upload_url.empty() ||
      !base::ReadFileToString(config_file, &config_text) ||
      config_text.empty()) {
    return;
  }

  std::unique_ptr<base::Value> value =
      base::JSONReader::ReadDeprecated(config_text);
  if (!value) {
    LOG(ERROR) << "Background tracing has incorrect config: " << config_text;
    return;
  }

  const base::DictionaryValue* dict = nullptr;
  if (!value->GetAsDictionary(&dict))
    return;

  std::unique_ptr<content::BackgroundTracingConfig> config =
      content::BackgroundTracingConfig::FromDict(dict);
  content::BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config),
      base::BindRepeating(&BackgroundTracingUploadCallback, upload_url),
      content::BackgroundTracingManager::NO_DATA_FILTERING);
}

}  // namespace

void SetConfigTextFilterForTesting(ConfigTextFilterForTesting predicate) {
  g_config_text_filter_for_testing = predicate;
}

void SetupBackgroundTracingFieldTrial() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableBackgroundTracing) &&
      command_line->HasSwitch(switches::kTraceUploadURL)) {
    tracing::SetupBackgroundTracingFromConfigFile(
        command_line->GetSwitchValuePath(switches::kEnableBackgroundTracing),
        command_line->GetSwitchValueASCII(switches::kTraceUploadURL));
    return;
  }

  std::unique_ptr<content::BackgroundTracingConfig> config =
      GetBackgroundTracingConfig();

  std::string upload_url = variations::GetVariationParamValue(
      kBackgroundTracingFieldTrial, kBackgroundTracingUploadUrl);
  content::BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config),
      base::BindRepeating(&BackgroundTracingUploadCallback, upload_url),
      content::BackgroundTracingManager::ANONYMIZE_DATA);
}

}  // namespace tracing
