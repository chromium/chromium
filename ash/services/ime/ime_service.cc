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
  if ((connection_factory_ && connection_factory_->IsConnected()) ||
      (input_engine_ && input_engine_->IsConnected())) {
    std::move(callback).Run(/*bound=*/false);
    return;
  }

  input_engine_.reset();
  decoder_engine_ = std::make_unique<DecoderEngine>(
      this, ImeDecoder::GetInstance()->GetEntryPoints());
  bool bound = decoder_engine_->BindRequest(
      ime_spec, std::move(to_engine_request), std::move(from_engine), extra);
  std::move(callback).Run(bound);
}

void ImeService::ConnectToInputMethod(
    const std::string& ime_spec,
    mojo::PendingReceiver<mojom::InputMethod> input_method,
    mojo::PendingRemote<mojom::InputMethodHost> input_method_host,
    ConnectToInputMethodCallback callback) {
  // DEPRECATED: This method is now deprecated. If you would like to connect to
  //   an input method in the ime service, please do so via the
  //   InitializeConnectionFactory method below.
  std::move(callback).Run(/*bound=*/false);
}

void ImeService::InitializeConnectionFactory(
    mojo::PendingReceiver<mojom::ConnectionFactory> connection_factory,
    mojom::ConnectionTarget connection_target,
    InitializeConnectionFactoryCallback callback) {
  // Drop any currently bound pipes.
  connection_factory_.reset();
  decoder_engine_.reset();
  input_engine_.reset();

  if (connection_target == mojom::ConnectionTarget::kImeService) {
    connection_factory_ =
        std::make_unique<ConnectionFactory>(std::move(connection_factory));
    std::move(callback).Run(/*success=*/true);
    return;
  }

  auto system_engine = std::make_unique<SystemEngine>(
      this, ImeDecoder::GetInstance()->GetEntryPoints());
  bool bound =
      system_engine->BindConnectionFactory(std::move(connection_factory));
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
  if (strcmp(feature_name, "SystemLatinPhysicalTyping") == 0) {
    return true;
  }
  if (strcmp(feature_name, "SystemTransliterationPhysicalTyping") == 0) {
    return base::FeatureList::IsEnabled(
        chromeos::features::kSystemTransliterationPhysicalTyping);
  }
  return false;
}

void ImeService::Unused2() {
  NOTIMPLEMENTED();
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

void ImeService::Unused1() {
  NOTIMPLEMENTED();
}

}  // namespace ime
}  // namespace chromeos
