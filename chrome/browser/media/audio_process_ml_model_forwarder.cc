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
class AudioProcessMlModelForwarder::AudioProcessObserver
    : content::ServiceProcessHost::Observer {
 public:
  using ServiceLaunchCallback =
      base::RepeatingCallback<void(mojo::Remote<audio::mojom::MlModelManager>)>;

  AudioProcessObserver() = default;

  ~AudioProcessObserver() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (launch_callback_) {
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
      pref_service_(pref_service),
      background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (audio_process_observer_) {
    // base::Unretained is safe since `this` owns and outlives the observer.
    audio_process_observer_->Start(base::BindRepeating(
        &AudioProcessMlModelForwarder::OnAudioProcessLaunched,
        base::Unretained(this)));
  }
}

AudioProcessMlModelForwarder::~AudioProcessMlModelForwarder() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void AudioProcessMlModelForwarder::Initialize(
    optimization_guide::OptimizationGuideModelProvider& model_provider) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(!model_observation_);
  model_observation_.emplace(&model_provider, background_task_runner_,
                             /*observer=*/this);

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

  MaybeRegisterModelObserver();
}

void AudioProcessMlModelForwarder::OnAudioCaptureStarted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  audio_input_stream_creation_observed_ = true;
  if (pref_service_) {
    // Store the time of this event for checking at initialization.
    pref_service_->SetTime(prefs::kAudioInputStreamLastTimeCreated,
                           base::Time::Now());
  }
  MaybeRegisterModelObserver();
}

void AudioProcessMlModelForwarder::CancelModelLoadingTasks() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  weak_factory_.InvalidateWeakPtrs();
}

void AudioProcessMlModelForwarder::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  model_path_ = model_info.has_value() ? model_info->GetModelFilePath()
                                       : base::FilePath();
  if (model_path_.empty() && audio_process_model_manager_) {
    CancelModelLoadingTasks();
    audio_process_model_manager_->StopServingModel(
        audio::mojom::MlModelType::kResidualEchoEstimation);
    return;
  }
  MaybeSendModelToAudioProcess();
}

void AudioProcessMlModelForwarder::OnAudioProcessLaunched(
    mojo::Remote<audio::mojom::MlModelManager> model_manager) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  audio_process_model_manager_ = std::move(model_manager);
  audio_process_model_manager_.reset_on_disconnect();

  // Call MaybeSendModelToAudioProcess before registering, to avoid scheduling
  // double file open tasks in the case when a model is immediately available.
  MaybeSendModelToAudioProcess();
  MaybeRegisterModelObserver();
}

void AudioProcessMlModelForwarder::MaybeRegisterModelObserver() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Avoid registering the model observer until we have an indication that a
  // model is likely to be used. This reduces unnecessary model downloads.
  if (!audio_input_stream_creation_observed_) {
    return;
  }
  if (model_observation_ && !model_observation_->IsRegistered()) {
    model_observation_->Observe(
        optimization_guide::proto::
            OPTIMIZATION_TARGET_WEBRTC_NEURAL_RESIDUAL_ECHO_ESTIMATOR,
        /*model_metadata=*/std::nullopt);
  }
}

void AudioProcessMlModelForwarder::MaybeSendModelToAudioProcess() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  CancelModelLoadingTasks();

  if (!audio_process_model_manager_) {
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
      base::BindOnce(&AudioProcessMlModelForwarder::OnModelFileOpened,
                     weak_factory_.GetWeakPtr()));
}

void AudioProcessMlModelForwarder::OnModelFileOpened(WrappedFilePtr file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!file || !audio_process_model_manager_) {
    // No file or nowhere to send it.
    return;
  }
  audio_process_model_manager_->SetModel(
      audio::mojom::MlModelType::kResidualEchoEstimation, std::move(*file));
}
