// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/ime/ime_service.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/services/ime/constants.h"
#include "ash/services/ime/decoder/decoder_engine.h"
#include "ash/services/ime/decoder/system_engine.h"
#include "ash/services/ime/rule_based_engine.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/c/system/thunks.h"

namespace chromeos {
namespace ime {

namespace {

enum SimpleDownloadError {
  SIMPLE_DOWNLOAD_ERROR_OK = 0,
  SIMPLE_DOWNLOAD_ERROR_FAILED = -1,
  SIMPLE_DOWNLOAD_ERROR_ABORTED = -2,
};

// Compose a relative FilePath beased on a C-string path.
base::FilePath RelativePathFromCStr(const char* path) {
  // Target path MUST be relative for security concerns.
  base::FilePath initial_path(path);
  base::FilePath relative_path(kInputMethodsDirName);
  return relative_path.Append(kLanguageDataDirName)
      .Append(initial_path.BaseName());
}

// Convert a final downloaded file path to a allowlisted path in string format.
std::string ResolveDownloadPath(const base::FilePath& file) {
  base::FilePath target(kUserInputMethodsDirPath);
  target = target.Append(kLanguageDataDirName).Append(file.BaseName());
  return target.MaybeAsASCII();
}

bool IsRuleBasedInputMethod(const std::string& engine_id) {
  return base::StartsWith(engine_id, "m17n:", base::CompareCase::SENSITIVE);
}

}  // namespace

ImeService::ImeService(mojo::PendingReceiver<mojom::ImeService> receiver)
    : receiver_(this, std::move(receiver)),
      main_task_runner_(base::SequencedTaskRunnerHandle::Get()) {
}

ImeService::~ImeService() = default;

void ImeService::SetPlatformAccessProvider(
    mojo::PendingRemote<mojom::PlatformAccessProvider> provider) {
  platform_access_.Bind(std::move(provider));
}

void ImeService::BindInputEngineManager(
    mojo::PendingReceiver<mojom::InputEngineManager> receiver) {
  manager_receivers_.Add(this, std::move(receiver));
}

void ImeService::ConnectToImeEngine(
    const std::string& ime_spec,
    mojo::PendingReceiver<mojom::InputChannel> to_engine_request,
    mojo::PendingRemote<mojom::InputChannel> from_engine,
    const std::vector<uint8_t>& extra,
    ConnectToImeEngineCallback callback) {
  // There can only be one client using the decoder at any time. There are two
  // possible clients: NativeInputMethodEngine (for physical keyboard) and the
  // XKB extension (for virtual keyboard). The XKB extension may try to
  // connect the decoder even when it's not supposed to (due to race
  // conditions), so we must prevent the extension from taking over the
  // NativeInputMethodEngine connection.
  //
  // The extension will only use ConnectToImeEngine, and NativeInputMethodEngine
  // will only use ConnectToInputMethod.
  if (input_engine_ && input_engine_->IsConnected()) {
    std::move(callback).Run(/*bound=*/false);
    return;
  }

  input_engine_.reset();
  decoder_engine_ = std::make_unique<DecoderEngine>(this);
  bool bound = decoder_engine_->BindRequest(
      ime_spec, std::move(to_engine_request), std::move(from_engine), extra);
  std::move(callback).Run(bound);
}

void ImeService::ConnectToInputMethod(
    const std::string& ime_spec,
    mojo::PendingReceiver<mojom::InputMethod> input_method,
    mojo::PendingRemote<mojom::InputMethodHost> input_method_host,
    ConnectToInputMethodCallback callback) {
  decoder_engine_.reset();

  if (IsRuleBasedInputMethod(ime_spec)) {
    input_engine_ = RuleBasedEngine::Create(ime_spec, std::move(input_method),
                                            std::move(input_method_host));
    std::move(callback).Run(/*bound=*/input_engine_ != nullptr);
    return;
  }

  auto system_engine = std::make_unique<SystemEngine>(this);
  bool bound = system_engine->BindRequest(ime_spec, std::move(input_method),
                                          std::move(input_method_host));
  input_engine_ = std::move(system_engine);
  std::move(callback).Run(bound);
}

const char* ImeService::GetImeBundleDir() {
  return kBundledInputMethodsDirPath;
}

const char* ImeService::GetImeGlobalDir() {
  return "";
}

const char* ImeService::GetImeUserHomeDir() {
  return kUserInputMethodsDirPath;
}

void ImeService::RunInMainSequence(ImeSequencedTask task, int task_id) {
  // This helps ensure that tasks run in the **main** SequencedTaskRunner.
  // E.g. the Mojo Remotes are bound on the main_task_runner_, so that any task
  // invoked Mojo call from other threads (sequences) should be posted to
  // main_task_runner_ by this function.
  main_task_runner_->PostTask(FROM_HERE, base::BindOnce(task, task_id));
}

bool ImeService::IsFeatureEnabled(const char* feature_name) {
  if (strcmp(feature_name, "AssistiveEmojiEnhanced") == 0) {
    return base::FeatureList::IsEnabled(
        chromeos::features::kAssistEmojiEnhanced);
  }
  if (strcmp(feature_name, "AssistiveMultiWord") == 0) {
    return chromeos::features::IsAssistiveMultiWordEnabled();
  }
  if (strcmp(feature_name, "AssistiveMultiWordLacrosSupport") == 0) {
    return base::FeatureList::IsEnabled(
               chromeos::features::kAssistMultiWordLacrosSupport) &&
           chromeos::features::IsAssistiveMultiWordEnabled();
  }
  if (strcmp(feature_name, "LacrosSupport") == 0) {
    return base::FeatureList::IsEnabled(chromeos::features::kLacrosSupport);
  }
  if (strcmp(feature_name, "SystemChinesePhysicalTyping") == 0) {
    return features::IsSystemChinesePhysicalTypingEnabled();
  }
  if (strcmp(feature_name, "SystemJapanesePhysicalTyping") == 0) {
    return features::IsSystemJapanesePhysicalTypingEnabled();
  }
  if (strcmp(feature_name, "SystemKoreanPhysicalTyping") == 0) {
    return features::IsSystemKoreanPhysicalTypingEnabled();
  }
  if (strcmp(feature_name, "SystemLatinPhysicalTyping") == 0) {
    return true;
  }
  return false;
}

int ImeService::SimpleDownloadToFile(const char* url,
                                     const char* file_path,
                                     SimpleDownloadCallback callback) {
  if (!platform_access_.is_bound()) {
    callback(SIMPLE_DOWNLOAD_ERROR_ABORTED, "");
    LOG(ERROR) << "Failed to download due to missing binding.";
  } else {
    platform_access_->DownloadImeFileTo(
        GURL(url), RelativePathFromCStr(file_path),
        base::BindOnce(&ImeService::SimpleDownloadFinished,
                       base::Unretained(this), std::move(callback)));
  }

  // For |SimpleDownloadToFile|, always returns 0.
  return 0;
}

void ImeService::SimpleDownloadFinished(SimpleDownloadCallback callback,
                                        const base::FilePath& file) {
  if (file.empty()) {
    callback(SIMPLE_DOWNLOAD_ERROR_FAILED, "");
  } else {
    callback(SIMPLE_DOWNLOAD_ERROR_OK, ResolveDownloadPath(file).c_str());
  }
}

int ImeService::SimpleDownloadToFileV2(const char* url,
                                       const char* file_path,
                                       SimpleDownloadCallbackV2 callback) {
  if (!platform_access_.is_bound()) {
    callback(SIMPLE_DOWNLOAD_ERROR_ABORTED, url, "");
    LOG(ERROR) << "Failed to download due to missing binding.";
  } else {
    platform_access_->DownloadImeFileTo(
        GURL(url), RelativePathFromCStr(file_path),
        base::BindOnce(&ImeService::SimpleDownloadFinishedV2,
                       base::Unretained(this), std::move(callback),
                       std::string(url)));
  }

  // For |SimpleDownloadToFileV2|, always returns 0.
  return 0;
}

void ImeService::SimpleDownloadFinishedV2(SimpleDownloadCallbackV2 callback,
                                          const std::string& url_str,
                                          const base::FilePath& file) {
  if (file.empty()) {
    callback(SIMPLE_DOWNLOAD_ERROR_FAILED, url_str.c_str(), "");
  } else {
    callback(SIMPLE_DOWNLOAD_ERROR_OK, url_str.c_str(),
             ResolveDownloadPath(file).c_str());
  }
}

const MojoSystemThunks* ImeService::GetMojoSystemThunks() {
  return MojoEmbedderGetSystemThunks();
}

ImeCrosDownloader* ImeService::GetDownloader() {
  // TODO(https://crbug.com/837156): Create an ImeCrosDownloader based on its
  // specification defined in interfaces. The caller should free it after use.
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace ime
}  // namespace chromeos
