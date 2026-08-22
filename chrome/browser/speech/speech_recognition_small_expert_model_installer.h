// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_SMALL_EXPERT_MODEL_INSTALLER_H_
#define CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_SMALL_EXPERT_MODEL_INSTALLER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/on_device_model/public/mojom/download_observer.mojom.h"

namespace optimization_guide {
class ModelBrokerClient;
class ModelClient;
}  // namespace optimization_guide

class PrefRegistrySimple;
class Profile;

namespace speech {

class SpeechRecognitionSmallExpertModelInstaller
    : public on_device_model::mojom::DownloadObserver {
 public:
  enum class SpeechRecognitionSmallExpertModelState {
    kNotInstalled = 0,
    kDownloading = 1,
    kInstalling = 2,
    kInstalled = 3,
    kError = 4,
    kErrorCorruptPersistent = 5,
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnSpeechRecognitionSmallExpertModelInstalled() {}
    virtual void OnSpeechRecognitionSmallExpertModelInstallError() {}
    virtual void OnSpeechRecognitionSmallExpertModelProgress(
        int progress,
        double bytes_per_sec,
        base::TimeDelta eta) {}
    virtual void OnSpeechRecognitionSmallExpertModelStateChanged(
        SpeechRecognitionSmallExpertModelState state) {}
  };

  SpeechRecognitionSmallExpertModelInstaller();
  ~SpeechRecognitionSmallExpertModelInstaller() override;
  SpeechRecognitionSmallExpertModelInstaller(
      const SpeechRecognitionSmallExpertModelInstaller&) = delete;
  SpeechRecognitionSmallExpertModelInstaller& operator=(
      const SpeechRecognitionSmallExpertModelInstaller&) = delete;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  virtual void InstallSpeechRecognitionSmallExpertModel(Profile* profile);
  virtual void UninstallSpeechRecognitionSmallExpertModel();
  virtual void TeardownSpeechRecognitionSmallExpertModel();
  virtual base::FilePath GetSpeechRecognitionSmallExpertModelPath() const;

  bool IsSpeechRecognitionSmallExpertModelInstalled() const;
  bool IsSpeechRecognitionSmallExpertModelDownloading() const;
  SpeechRecognitionSmallExpertModelState
  GetSpeechRecognitionSmallExpertModelState() const;
  int GetSpeechRecognitionSmallExpertModelRetryCount() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // on_device_model::mojom::DownloadObserver:
  void OnDownloadProgressUpdate(uint64_t downloaded_bytes,
                                uint64_t total_bytes) override;

  void NotifySpeechRecognitionSmallExpertModelInstalledForTesting();
  void NotifySpeechRecognitionSmallExpertModelErrorForTesting();
  void NotifySpeechRecognitionSmallExpertModelProgressForTesting(
      int progress,
      double speed = 0.0,
      base::TimeDelta eta = base::TimeDelta());
  void NotifySpeechRecognitionSmallExpertModelStateChangedForTesting(
      SpeechRecognitionSmallExpertModelState state);

 protected:
  void OnSpeechRecognitionSmallExpertModelInstalled(base::FilePath model_path);
  void OnSpeechRecognitionSmallExpertModelClientReady(
      base::WeakPtr<optimization_guide::ModelClient> client);
  void HandleSpeechRecognitionSmallExpertModelError();
  static void DeleteSpeechRecognitionSmallExpertModelFilesAsync(
      base::FilePath speech_recognition_small_expert_model_dir);

 private:
  void SetState(SpeechRecognitionSmallExpertModelState state);
  void OnSpeechRecognitionSmallExpertModelPathChecked(base::FilePath dir,
                                                      bool exists);

  base::ObserverList<Observer> observers_;
  SpeechRecognitionSmallExpertModelState
      speech_recognition_small_expert_model_state_ =
          SpeechRecognitionSmallExpertModelState::kNotInstalled;
  int speech_recognition_small_expert_model_retry_count_ = 0;

  std::unique_ptr<optimization_guide::ModelBrokerClient> model_broker_client_;
  mojo::Receiver<on_device_model::mojom::DownloadObserver>
      download_observer_receiver_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SpeechRecognitionSmallExpertModelInstaller>
      model_weak_factory_{this};
};

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_SMALL_EXPERT_MODEL_INSTALLER_H_
