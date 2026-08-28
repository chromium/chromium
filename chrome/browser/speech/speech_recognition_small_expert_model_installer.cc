// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_small_expert_model_installer.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/pref_names.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_broker_state.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "media/base/media_switches.h"
#include "services/on_device_model/public/cpp/buildflags.h"

namespace speech {

namespace {

using State = SpeechRecognitionSmallExpertModelInstaller::
    SpeechRecognitionSmallExpertModelState;

constexpr auto kFeature = optimization_guide::mojom::OnDeviceFeature::
    kSpeechRecognitionSmallExpertModel;

// Sentinel path stored in local state pref to indicate that the model is
// installed via Optimization Guide, which manages the on-disk storage and
// lifecycle of the model assets.
constexpr base::FilePath::CharType kOptimizationGuideSentinelPath[] =
    FILE_PATH_LITERAL("optimization_guide");
constexpr base::FilePath::CharType kSpeechRecognitionSmallExpertModelDirName[] =
    FILE_PATH_LITERAL("speech_recognition_small_expert_model");
constexpr int kMaxInstallRetries = 3;

PrefService* GetLocalState() {
  return g_browser_process ? g_browser_process->local_state() : nullptr;
}

bool IsLiveCaptionEnabled(Profile* profile) {
  if (!profile) {
    return false;
  }
  PrefService* prefs = profile->GetPrefs();
  return prefs && (prefs->GetBoolean(prefs::kLiveCaptionEnabled) ||
                   (captions::IsHeadlessCaptionFeatureSupported() &&
                    prefs->GetBoolean(prefs::kHeadlessCaptionEnabled)));
}

}  // namespace

// static
void SpeechRecognitionSmallExpertModelInstaller::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterFilePathPref(prefs::kSpeechRecognitionSmallExpertModelPath,
                                 base::FilePath());
}

SpeechRecognitionSmallExpertModelInstaller::
    SpeechRecognitionSmallExpertModelInstaller() {
  if (base::FeatureList::IsEnabled(
          media::kLiveCaptionSpeechRecognitionSmallExpertModel) &&
      !GetSpeechRecognitionSmallExpertModelPath().empty()) {
    speech_recognition_small_expert_model_state_ = State::kInstalled;
  }
}

SpeechRecognitionSmallExpertModelInstaller::
    ~SpeechRecognitionSmallExpertModelInstaller() = default;

void SpeechRecognitionSmallExpertModelInstaller::SetState(State state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (speech_recognition_small_expert_model_state_ == state) {
    return;
  }
  speech_recognition_small_expert_model_state_ = state;
  for (Observer& observer : observers_) {
    observer.OnSpeechRecognitionSmallExpertModelStateChanged(state);
  }
}

void SpeechRecognitionSmallExpertModelInstaller::
    InstallSpeechRecognitionSmallExpertModel(Profile* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::FeatureList::IsEnabled(
          media::kLiveCaptionSpeechRecognitionSmallExpertModel)) {
    return;
  }

  // Only attempt installation if not currently downloading/installing,
  // not already installed, and not permanently corrupt.
  if (speech_recognition_small_expert_model_state_ != State::kNotInstalled &&
      speech_recognition_small_expert_model_state_ != State::kError) {
    return;
  }

  model_weak_factory_.InvalidateWeakPtrs();

  if (!IsLiveCaptionEnabled(profile)) {
    SetState(State::kNotInstalled);
    return;
  }

  OptimizationGuideKeyedService* opt_guide =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          profile->GetOriginalProfile());
  if (!opt_guide) {
    SetState(State::kNotInstalled);
    return;
  }

  model_broker_client_ = opt_guide->CreateModelBrokerClient();
  if (!model_broker_client_) {
    HandleSpeechRecognitionSmallExpertModelError();
    return;
  }

  SetState(State::kDownloading);

  download_observer_receiver_.reset();
  model_broker_client_->AddModelDownloadProgressObserver(
      optimization_guide::ToUseCaseName(kFeature),
      download_observer_receiver_.BindNewPipeAndPassRemote());

  model_broker_client_->RequestAssetsFor(kFeature);
  model_broker_client_->GetSubscriber(kFeature).WaitForClient(
      base::BindOnce(&SpeechRecognitionSmallExpertModelInstaller::
                         OnSpeechRecognitionSmallExpertModelClientReady,
                     model_weak_factory_.GetWeakPtr()));
}

void SpeechRecognitionSmallExpertModelInstaller::
    OnSpeechRecognitionSmallExpertModelClientReady(
        base::WeakPtr<optimization_guide::ModelClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsSpeechRecognitionSmallExpertModelDownloading()) {
    return;
  }
  if (!client) {
    HandleSpeechRecognitionSmallExpertModelError();
    return;
  }
  OnSpeechRecognitionSmallExpertModelInstalled(
      base::FilePath(kOptimizationGuideSentinelPath));
}

void SpeechRecognitionSmallExpertModelInstaller::
    HandleSpeechRecognitionSmallExpertModelError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  download_observer_receiver_.reset();
  model_broker_client_.reset();
  speech_recognition_small_expert_model_retry_count_++;
  SetState(speech_recognition_small_expert_model_retry_count_ >=
                   kMaxInstallRetries
               ? State::kErrorCorruptPersistent
               : State::kError);
  for (Observer& observer : observers_) {
    observer.OnSpeechRecognitionSmallExpertModelInstallError();
  }
}

void SpeechRecognitionSmallExpertModelInstaller::OnDownloadProgressUpdate(
    uint64_t downloaded_bytes,
    uint64_t total_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (speech_recognition_small_expert_model_state_ != State::kDownloading ||
      total_bytes == 0) {
    return;
  }
  int progress = static_cast<int>(std::clamp(
      static_cast<double>(downloaded_bytes) / total_bytes * 100.0, 0.0, 100.0));

  for (Observer& observer : observers_) {
    observer.OnSpeechRecognitionSmallExpertModelProgress(progress, 0.0,
                                                         base::TimeDelta());
  }
}

void SpeechRecognitionSmallExpertModelInstaller::
    TeardownSpeechRecognitionSmallExpertModel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  model_weak_factory_.InvalidateWeakPtrs();
  download_observer_receiver_.reset();
  model_broker_client_.reset();
  speech_recognition_small_expert_model_retry_count_ = 0;
  SetState(State::kNotInstalled);
}

void SpeechRecognitionSmallExpertModelInstaller::
    UninstallSpeechRecognitionSmallExpertModel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::FilePath opt_path = GetSpeechRecognitionSmallExpertModelPath();

  if (PrefService* local_state = GetLocalState()) {
    local_state->ClearPref(prefs::kSpeechRecognitionSmallExpertModelPath);
  }

#if BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
  auto global_state =
      optimization_guide::OptimizationGuideGlobalState::CreateOrGet();
  if (global_state) {
    const std::string use_case = optimization_guide::ToUseCaseName(kFeature);
    auto& capability = global_state->on_device_capability();
    auto* manifest_state =
        static_cast<optimization_guide::ManifestBrokerState*>(&capability);
    manifest_state->SetUseCaseRequested(use_case, false);
  }
#endif

  TeardownSpeechRecognitionSmallExpertModel();

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&SpeechRecognitionSmallExpertModelInstaller::
                         DeleteSpeechRecognitionSmallExpertModelFilesAsync,
                     opt_path));
}

base::FilePath SpeechRecognitionSmallExpertModelInstaller::
    GetSpeechRecognitionSmallExpertModelPath() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (PrefService* local_state = GetLocalState()) {
    return local_state->GetFilePath(
        prefs::kSpeechRecognitionSmallExpertModelPath);
  }
  return base::FilePath();
}

bool SpeechRecognitionSmallExpertModelInstaller::
    IsSpeechRecognitionSmallExpertModelInstalled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return speech_recognition_small_expert_model_state_ == State::kInstalled;
}

bool SpeechRecognitionSmallExpertModelInstaller::
    IsSpeechRecognitionSmallExpertModelDownloading() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return speech_recognition_small_expert_model_state_ == State::kDownloading ||
         speech_recognition_small_expert_model_state_ == State::kInstalling;
}

SpeechRecognitionSmallExpertModelInstaller::
    SpeechRecognitionSmallExpertModelState
    SpeechRecognitionSmallExpertModelInstaller::
        GetSpeechRecognitionSmallExpertModelState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return speech_recognition_small_expert_model_state_;
}

int SpeechRecognitionSmallExpertModelInstaller::
    GetSpeechRecognitionSmallExpertModelRetryCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return speech_recognition_small_expert_model_retry_count_;
}

void SpeechRecognitionSmallExpertModelInstaller::AddObserver(
    Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void SpeechRecognitionSmallExpertModelInstaller::RemoveObserver(
    Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void SpeechRecognitionSmallExpertModelInstaller::
    NotifySpeechRecognitionSmallExpertModelInstalledForTesting() {  // IN-TEST
  SetState(State::kInstalled);
  for (Observer& observer : observers_) {
    observer.OnSpeechRecognitionSmallExpertModelInstalled();
  }
}

void SpeechRecognitionSmallExpertModelInstaller::
    NotifySpeechRecognitionSmallExpertModelErrorForTesting() {  // IN-TEST
  SetState(State::kError);
  for (Observer& observer : observers_) {
    observer.OnSpeechRecognitionSmallExpertModelInstallError();
  }
}

void SpeechRecognitionSmallExpertModelInstaller::
    NotifySpeechRecognitionSmallExpertModelProgressForTesting(  // IN-TEST
        int progress,
        double speed,
        base::TimeDelta eta) {
  for (Observer& observer : observers_) {
    observer.OnSpeechRecognitionSmallExpertModelProgress(progress, speed, eta);
  }
}

void SpeechRecognitionSmallExpertModelInstaller::
    NotifySpeechRecognitionSmallExpertModelStateChangedForTesting(  // IN-TEST
        State state) {
  SetState(state);
}

void SpeechRecognitionSmallExpertModelInstaller::
    OnSpeechRecognitionSmallExpertModelInstalled(base::FilePath model_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsSpeechRecognitionSmallExpertModelDownloading()) {
    return;
  }

  if (model_path.empty()) {
    HandleSpeechRecognitionSmallExpertModelError();
    return;
  }

  if (model_path == base::FilePath(kOptimizationGuideSentinelPath)) {
    OnSpeechRecognitionSmallExpertModelPathChecked(model_path, /*exists=*/true);
    return;
  }

  SetState(State::kInstalling);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&base::PathExists, model_path),
      base::BindOnce(&SpeechRecognitionSmallExpertModelInstaller::
                         OnSpeechRecognitionSmallExpertModelPathChecked,
                     model_weak_factory_.GetWeakPtr(), model_path));
}

void SpeechRecognitionSmallExpertModelInstaller::
    OnSpeechRecognitionSmallExpertModelPathChecked(base::FilePath dir,
                                                   bool exists) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsSpeechRecognitionSmallExpertModelDownloading()) {
    return;
  }

  if (dir.empty() || !exists) {
    HandleSpeechRecognitionSmallExpertModelError();
    return;
  }

  download_observer_receiver_.reset();
  model_broker_client_.reset();
  speech_recognition_small_expert_model_retry_count_ = 0;
  if (PrefService* local_state = GetLocalState()) {
    local_state->SetFilePath(prefs::kSpeechRecognitionSmallExpertModelPath,
                             dir);
  }
  SetState(State::kInstalled);
  for (Observer& observer : observers_) {
    observer.OnSpeechRecognitionSmallExpertModelInstalled();
  }
}

// static
void SpeechRecognitionSmallExpertModelInstaller::
    DeleteSpeechRecognitionSmallExpertModelFilesAsync(
        base::FilePath speech_recognition_small_expert_model_dir) {
  // Optimization Guide manages on-disk assets directly. Only delete files if
  // a custom absolute directory path was provided.
  if (speech_recognition_small_expert_model_dir.empty() ||
      !speech_recognition_small_expert_model_dir.IsAbsolute()) {
    return;
  }
  base::FilePath abs_path =
      base::MakeAbsoluteFilePath(speech_recognition_small_expert_model_dir);
  if (abs_path.empty()) {
    abs_path = speech_recognition_small_expert_model_dir;
  }
  if (!abs_path.ReferencesParent() &&
      abs_path.BaseName().value() ==
          kSpeechRecognitionSmallExpertModelDirName &&
      base::PathExists(abs_path)) {
    base::DeletePathRecursively(abs_path);
  }
}

}  // namespace speech
