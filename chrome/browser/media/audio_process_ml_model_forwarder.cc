// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/audio_process_ml_model_forwarder.h"

#include <memory>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/common/pref_names.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/delivery/optimization_target_model_observer.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_info.h"
#include "media/base/media_switches.h"
#include "services/audio/public/mojom/audio_service.mojom.h"

namespace {

// The maximum duration since audio input stream creation before we stop
// considering it a "recent" event.
//
// Used at initialization to check the last audio input stream creation time
// stored in `pref_service_`. If the time is recent, then we assume ML models
// for audio input stream processing are likely to be needed and
// AudioProcessMlModelForwarder proactively registers for model updates with
// the Optimization Guide.
//
// 30 days is chosen here to match:
// 1. the Optimization Guide model expiration time, and
// 2. similar retention heuristics in other features.
constexpr base::TimeDelta kRecentAudioCaptureThreshold = base::Days(30);

AudioProcessMlModelForwarder::WrappedFilePtr OpenFileAndReturn(
    base::FilePath path,
    scoped_refptr<base::SequencedTaskRunner> deletion_task_runner) {
  std::unique_ptr<base::File> file = std::make_unique<base::File>(
      path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                base::File::FLAG_WIN_SHARE_DELETE);
  if (!file->IsValid()) {
    return {nullptr, base::OnTaskRunnerDeleter(nullptr)};
  }
  return {file.release(), base::OnTaskRunnerDeleter(deletion_task_runner)};
}

}  // namespace

// Used to monitor audio process launches and bind to the audio service
// MlModelManager interface.
//
// This class must be accessed on the UI thread due to interaction with the
// ServiceProcessHost API and the Audio Service API. However, some unit tests
// (relying on TestingBrowserProcess) do not support such checks, so we only
// check content::BrowserThread::UI when the observer is explicitly started.
// When not started, we rely on the AudioProcessMlModelForwarder's sequence
// checks.
class AudioProcessMlModelForwarder::AudioProcessObserver
    : content::ServiceProcessHost::Observer {
 public:
  using ServiceLaunchCallback =
      base::RepeatingCallback<void(mojo::Remote<audio::mojom::MlModelManager>)>;

  AudioProcessObserver() = default;

  ~AudioProcessObserver() override {
    // For `launch_callback_` data races, we rely on the
    // AudioProcessMlModelForwarder to sequence check all accesses to the
    // AudioProcessObserver.
    if (launch_callback_) {
      DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
      content::ServiceProcessHost::RemoveObserver(this);
    }
  }

  void Start(ServiceLaunchCallback cb) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    launch_callback_ = std::move(cb);
    content::ServiceProcessHost::AddObserver(this);
    // Trigger immediately if the audio process is already running.
    for (const auto& info :
         content::ServiceProcessHost::GetRunningProcessInfo()) {
      if (info.IsService<audio::mojom::AudioService>()) {
        OnServiceProcessLaunched(info);
        break;
      }
    }
  }

  void OnServiceProcessLaunched(
      const content::ServiceProcessInfo& info) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!info.IsService<audio::mojom::AudioService>()) {
      return;
    }
    mojo::Remote<audio::mojom::MlModelManager> model_manager;
    content::GetAudioService().BindMlModelManager(
        model_manager.BindNewPipeAndPassReceiver());
    launch_callback_.Run(std::move(model_manager));
  }

 private:
  ServiceLaunchCallback launch_callback_;
};

// Used to monitor audio capture requests and notify the forwarder when device
// audio capture streams are opened.
class AudioProcessMlModelForwarder::AudioCaptureRequestObserver
    : public MediaCaptureDevicesDispatcher::Observer {
 public:
  explicit AudioCaptureRequestObserver(AudioProcessMlModelForwarder& forwarder)
      : forwarder_(forwarder) {
    MediaCaptureDevicesDispatcher::GetInstance()->AddObserver(this);
  }

  ~AudioCaptureRequestObserver() override {
    MediaCaptureDevicesDispatcher::GetInstance()->RemoveObserver(this);
  }

  // MediaCaptureDevicesDispatcher::Observer:
  void OnRequestUpdate(int render_process_id,
                       int render_frame_id,
                       blink::mojom::MediaStreamType stream_type,
                       const content::MediaRequestState state) override {
    if (state != content::MEDIA_REQUEST_STATE_DONE) {
      return;
    }
    // Only react to device audio capture, where echo cancellation may run in
    // the audio process.
    if (stream_type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
      forwarder_->OnAudioCaptureStarted();
    }
  }

 private:
  const base::raw_ref<AudioProcessMlModelForwarder> forwarder_;
};

// static
std::unique_ptr<AudioProcessMlModelForwarder>
AudioProcessMlModelForwarder::Create(PrefService* pref_service) {
  // Using `new` to access a non-public constructor.
  return base::WrapUnique(new AudioProcessMlModelForwarder(
      std::make_unique<AudioProcessObserver>(), pref_service));
}

// static
std::unique_ptr<AudioProcessMlModelForwarder>
AudioProcessMlModelForwarder::CreateWithoutAudioProcessObserverForTesting(
    PrefService* pref_service) {
  // Using `new` to access a non-public constructor.
  return base::WrapUnique(new AudioProcessMlModelForwarder(
      /*audio_process_observer=*/nullptr, pref_service));
}

AudioProcessMlModelForwarder::AudioProcessMlModelForwarder(
    std::unique_ptr<AudioProcessObserver> audio_process_observer,
    PrefService* pref_service)
    : audio_process_observer_(std::move(audio_process_observer)),
      pref_service_(pref_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (base::FeatureList::IsEnabled(
          media::kWebRtcAudioNeuralResidualEchoEstimation)) {
    model_forwarders_[audio::mojom::MlModelType::kResidualEchoEstimation] =
        std::make_unique<SingleModelForwarder>(
            optimization_guide::proto::
                OPTIMIZATION_TARGET_WEBRTC_NEURAL_RESIDUAL_ECHO_ESTIMATOR,
            audio::mojom::MlModelType::kResidualEchoEstimation, /*owner=*/this);
  }
  if (base::FeatureList::IsEnabled(media::kWebRtcVoiceIsolationDenoiser)) {
    model_forwarders_[audio::mojom::MlModelType::kVoiceIsolationDenoiser] =
        std::make_unique<SingleModelForwarder>(
            optimization_guide::proto::
                OPTIMIZATION_TARGET_WEBRTC_VOICE_ISOLATION_DENOISER,
            audio::mojom::MlModelType::kVoiceIsolationDenoiser, /*owner=*/this);
  }
}

AudioProcessMlModelForwarder::~AudioProcessMlModelForwarder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

AudioProcessMlModelForwarder::SingleModelForwarder::SingleModelForwarder(
    optimization_guide::proto::OptimizationTarget target,
    audio::mojom::MlModelType mojo_type,
    AudioProcessMlModelForwarder* owner)
    : target_(target), mojo_type_(mojo_type), owner_(owner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

AudioProcessMlModelForwarder::SingleModelForwarder::~SingleModelForwarder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void AudioProcessMlModelForwarder::SingleModelForwarder::Initialize(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!model_observation_);
  background_task_runner_ = background_task_runner;
  model_observation_.emplace(model_provider, background_task_runner_, this);
}

void AudioProcessMlModelForwarder::SingleModelForwarder::
    MaybeRegisterModelObserver(bool audio_input_stream_creation_observed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Avoid registering the model observer until we have an indication that a
  // model is likely to be used. This reduces unnecessary model downloads.
  if (!audio_input_stream_creation_observed) {
    return;
  }
  if (model_observation_ && !model_observation_->IsRegistered()) {
    model_observation_->Observe(target_, /*model_metadata=*/std::nullopt);
  }
}

void AudioProcessMlModelForwarder::SingleModelForwarder::
    CancelModelLoadingTasks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
}

void AudioProcessMlModelForwarder::SingleModelForwarder::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(optimization_target, target_);
  model_path_ =
      model_info.has_value() ? model_info->model_file_path : base::FilePath();
  if (model_path_.empty() && owner_->audio_process_model_manager_) {
    CancelModelLoadingTasks();
    owner_->audio_process_model_manager_->StopServingModel(mojo_type_);
    return;
  }
  MaybeSendModelToAudioProcess();
}

void AudioProcessMlModelForwarder::SingleModelForwarder::
    MaybeSendModelToAudioProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CancelModelLoadingTasks();

  if (!owner_->audio_process_model_manager_) {
    // No audio process to forward to.
    return;
  }
  if (model_path_.empty()) {
    // No model to forward.
    return;
  }
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&OpenFileAndReturn, model_path_, background_task_runner_),
      base::BindOnce(&AudioProcessMlModelForwarder::SingleModelForwarder::
                         OnModelFileOpened,
                     weak_factory_.GetWeakPtr()));
}

void AudioProcessMlModelForwarder::SingleModelForwarder::OnModelFileOpened(
    WrappedFilePtr file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!file || !owner_->audio_process_model_manager_) {
    // No file or nowhere to send it.
    return;
  }
  owner_->audio_process_model_manager_->SetModel(mojo_type_, std::move(*file));
}

void AudioProcessMlModelForwarder::Initialize(
    optimization_guide::OptimizationGuideModelProvider& model_provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (audio_process_observer_) {
    // base::Unretained is safe since `this` owns and outlives the observer.
    audio_process_observer_->Start(base::BindRepeating(
        &AudioProcessMlModelForwarder::OnAudioProcessLaunched,
        base::Unretained(this)));
  }
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  for (auto& [_, forwarder] : model_forwarders_) {
    forwarder->Initialize(&model_provider, background_task_runner);
  }

  if (pref_service_) {
    base::Time last_audio_input_stream_creation_time =
        pref_service_->GetTime(prefs::kAudioInputStreamLastTimeCreated);
    if (!last_audio_input_stream_creation_time.is_null() &&
        base::Time::Now() - last_audio_input_stream_creation_time <=
            kRecentAudioCaptureThreshold) {
      audio_input_stream_creation_observed_ = true;
    }
  }
  audio_capture_request_observer_ =
      std::make_unique<AudioCaptureRequestObserver>(*this);

  MaybeRegisterModelObservers();
}

void AudioProcessMlModelForwarder::OnAudioCaptureStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  audio_input_stream_creation_observed_ = true;
  if (pref_service_) {
    // Store the time of this event for checking at initialization.
    pref_service_->SetTime(prefs::kAudioInputStreamLastTimeCreated,
                           base::Time::Now());
  }
  MaybeRegisterModelObservers();
}

void AudioProcessMlModelForwarder::OnAudioProcessLaunched(
    mojo::Remote<audio::mojom::MlModelManager> model_manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  audio_process_model_manager_ = std::move(model_manager);
  audio_process_model_manager_.reset_on_disconnect();

  // Call MaybeSendModelsToAudioProcess() before registering, to avoid
  // scheduling double file open tasks in the case when a model is immediately
  // available.
  MaybeSendModelsToAudioProcess();
  MaybeRegisterModelObservers();
}

void AudioProcessMlModelForwarder::MaybeRegisterModelObservers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& [_, forwarder] : model_forwarders_) {
    forwarder->MaybeRegisterModelObserver(
        audio_input_stream_creation_observed_);
  }
}

void AudioProcessMlModelForwarder::MaybeSendModelsToAudioProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& [_, forwarder] : model_forwarders_) {
    forwarder->MaybeSendModelToAudioProcess();
  }
}
