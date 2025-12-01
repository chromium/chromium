// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_AUDIO_PROCESS_ML_MODEL_FORWARDER_H_
#define CHROME_BROWSER_MEDIA_AUDIO_PROCESS_ML_MODEL_FORWARDER_H_

#include <memory>
#include <optional>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/optional_ref.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/delivery/optimization_target_model_observer.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/mojom/ml_model_manager.mojom.h"

namespace optimization_guide {
class ModelInfo;
}  // namespace optimization_guide

class AudioProcessObserver;

// Propagates ML models from the Optimization Guide to the audio process.
// Currently only a single model, for residual echo estimatino, but may be
// extended to other optimization targets in the future.
//
// Does nothing until both an Optimization Guide model provider has been set and
// the audio process is launched. Then, observes updates from the model provider
// and forwards them to the audio process.
//
// NOTE: This class only forwards models to the audio process, i.e., not when
// the audio service is running as a part of the browser process. Models are
// currently not expected to be used when running in the browser process.
//
// Lives on the UI thread.
class AudioProcessMlModelForwarder
    : public optimization_guide::OptimizationTargetModelObserver {
 public:
  using WrappedFilePtr = std::unique_ptr<base::File, base::OnTaskRunnerDeleter>;

  // Default factory function. Monitors audio service process launches via
  // global APIs.
  static std::unique_ptr<AudioProcessMlModelForwarder> Create();

  // Testing factory function. Expects audio service process launches to be
  // signaled by calls to OnAudioProcessLaunched().
  static std::unique_ptr<AudioProcessMlModelForwarder>
  CreateWithoutAudioProcessObserverForTesting();

  ~AudioProcessMlModelForwarder() override;

  // Set the Optimization Guide model provider. May only be called once. The
  // model provider must outlive the AudioProcessMlModelForwarder.
  void Initialize(
      optimization_guide::OptimizationGuideModelProvider& model_provider);

  // Needs to be called with a remote for the audio service, in order to forward
  // model updates. May be called more than once to set a new remote, e.g., on
  // service restarts.
  void OnAudioProcessLaunched(
      mojo::Remote<audio::mojom::MlModelManager> ml_model_manager);

  bool HasPendingTasksForTesting() const { return weak_factory_.HasWeakPtrs(); }
  bool HasBoundAudioProcessRemoteForTesting() const {
    return audio_process_model_manager_.is_bound();
  }

 private:
  // optimization_guide::OptimizationTargetModelObserver:
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info)
      override;

  // If `audio_process_observer` is null, the forwarder does not handle audio
  // service process monitoring internally. See Create*() for details.
  explicit AudioProcessMlModelForwarder(
      std::unique_ptr<AudioProcessObserver> audio_process_observer);

  // Stops any ongoing loading of models.
  void CancelModelLoadingTasks();

  // Register model observation if the audio process has been launched and a
  // model provider is available.
  //
  // NOTE: OnModelUpdated() may be called immediately upon registering, even
  // within the call to MaybeRegisterModelObserver().
  void MaybeRegisterModelObserver();

  void MaybeSendModelToAudioProcess();

  // Continuation for MaybeSendModelToAudioProcess(), expecting either a nullptr
  // or an open, valid model file.
  void OnModelFileOpened(WrappedFilePtr file);

  // Signals when the audio process is ready to start receiving model updates.
  const std::unique_ptr<AudioProcessObserver> audio_process_observer_;

  // Task runner for loading model files.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Handle for passing models to the audio process.
  mojo::Remote<audio::mojom::MlModelManager> audio_process_model_manager_;

  // Latest update from the model provider. Empty if no path received yet or if
  // a nullopt model update has been received.
  base::FilePath model_path_;

  // Handles registration / deregistration with the model delivery framework.
  std::optional<optimization_guide::OptimizationGuideModelProviderObservation>
      model_observation_;

  base::WeakPtrFactory<AudioProcessMlModelForwarder> weak_factory_{this};
};

#endif  // CHROME_BROWSER_MEDIA_AUDIO_PROCESS_ML_MODEL_FORWARDER_H_
