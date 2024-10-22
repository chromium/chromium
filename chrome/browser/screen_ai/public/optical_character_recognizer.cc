// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/public/optical_character_recognizer.h"

#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "content/public/browser/browser_thread.h"

namespace {

class SequenceBoundReceiver {
 public:
  SequenceBoundReceiver() = default;
  ~SequenceBoundReceiver() = default;

  void OnReceivedOCR(
      base::OnceCallback<void(screen_ai::mojom::VisualAnnotationPtr)> callback,
      screen_ai::mojom::VisualAnnotationPtr visual_annotation) {
    std::move(callback).Run(std::move(visual_annotation));
  }
};

void RunCallback(base::OnceCallback<void(bool)> callback, bool result) {
  if (callback.is_null()) {
    return;
  }
  std::move(callback).Run(result);
}

}  // namespace

namespace screen_ai {

// static
scoped_refptr<screen_ai::OpticalCharacterRecognizer>
OpticalCharacterRecognizer::Create(Profile* profile,
                                   mojom::OcrClientType client_type) {
  CHECK(profile);
  return CreateWithStatusCallback(profile, client_type,
                                  base::NullCallbackAs<void(bool)>());
}

// static
scoped_refptr<screen_ai::OpticalCharacterRecognizer>
OpticalCharacterRecognizer::CreateWithStatusCallback(
    Profile* profile,
    mojom::OcrClientType client_type,
    base::OnceCallback<void(bool)> status_callback) {
  CHECK(profile);
  auto ocr = base::MakeRefCounted<screen_ai::OpticalCharacterRecognizer>(
      profile, client_type);
  ocr->Initialize(std::move(status_callback));
  return ocr;
}

OpticalCharacterRecognizer::OpticalCharacterRecognizer(
    Profile* profile,
    mojom::OcrClientType client_type)
    : RefCountedDeleteOnSequence<OpticalCharacterRecognizer>(
          content::GetUIThreadTaskRunner()),
      profile_(profile),
      client_type_(client_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Tests may pass an empty profile.
  if (profile_) {
    profile_observer_.Observe(profile_);
  }
}

void OpticalCharacterRecognizer::Initialize(
    base::OnceCallback<void(bool)> status_callback) {
  if (!profile_) {
    RunCallback(std::move(status_callback), false);
    ready_ = false;
    return;
  }

  ScreenAIServiceRouter* router =
      ScreenAIServiceRouterFactory::GetForBrowserContext(profile_);

  // Trigger service initialization to get a feedback when it's ready.
  scoped_refptr<OpticalCharacterRecognizer> ref_ptr(this);
  router->GetServiceStateAsync(
      ScreenAIServiceRouter::Service::kOCR,
      base::BindOnce(&OpticalCharacterRecognizer::OnOCRInitializationCallback,
                     ref_ptr, std::move(status_callback)));
}

void OpticalCharacterRecognizer::OnOCRInitializationCallback(
    base::OnceCallback<void(bool)> status_callback,
    bool successful) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RunCallback(std::move(status_callback), successful && profile_);

  // If the profile is already destroyed, stop here.
  if (!profile_) {
    ready_ = false;
    return;
  }

  // This should be called only once.
  DCHECK(!is_ready());
  ready_ = successful;
}

void OpticalCharacterRecognizer::MaybeConnectToOcrService() {
  if (is_connected()) {
    return;
  }

  if (!screen_ai_annotator_) {
    screen_ai_annotator_ =
        std::make_unique<mojo::Remote<mojom::ScreenAIAnnotator>>();
  }
  ScreenAIServiceRouterFactory::GetForBrowserContext(profile_)
      ->BindScreenAIAnnotator(
          screen_ai_annotator_->BindNewPipeAndPassReceiver());
  screen_ai_annotator_->reset_on_disconnect();
  (*screen_ai_annotator_)->SetClientType(client_type_);
}

void OpticalCharacterRecognizer::OnProfileWillBeDestroyed(Profile* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (screen_ai_annotator_) {
    screen_ai_annotator_.reset();
  }
  profile_ = nullptr;
  ready_ = false;
  profile_observer_.Reset();
}

OpticalCharacterRecognizer::~OpticalCharacterRecognizer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void OpticalCharacterRecognizer::PerformOCR(
    const ::SkBitmap& image,
    base::OnceCallback<void(screen_ai::mojom::VisualAnnotationPtr)> callback) {
  if (!is_ready()) {
    VLOG(0)
        << "PerformOCR called before the service is ready, returning empty.";
    std::move(callback).Run(mojom::VisualAnnotation::New());
    return;
  }

  if (::content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    MaybeConnectToOcrService();
    (*screen_ai_annotator_)
        ->PerformOcrAndReturnAnnotation(image, std::move(callback));
    return;
  }

  // This function can be called on a different sequence, but only on one
  // sequence other than the UI thread.
  if (!sequence_bound_receiver_) {
    sequence_bound_receiver_ =
        std::make_unique<base::SequenceBound<SequenceBoundReceiver>>(
            base::SequencedTaskRunner::GetCurrentDefault());
  }

  scoped_refptr<OpticalCharacterRecognizer> ref_ptr(this);
  content::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::BindOnce(
          [](scoped_refptr<OpticalCharacterRecognizer> ocr,
             const SkBitmap& image,
             base::OnceCallback<void(mojom::VisualAnnotationPtr)> callback) {
            ocr->MaybeConnectToOcrService();
            (*ocr->screen_ai_annotator_)
                ->PerformOcrAndReturnAnnotation(image, std::move(callback));
          },
          ref_ptr, std::move(image),
          base::BindOnce(
              [](scoped_refptr<OpticalCharacterRecognizer> ocr,
                 base::OnceCallback<void(mojom::VisualAnnotationPtr)> callback,
                 screen_ai::mojom::VisualAnnotationPtr visual_annotation) {
                ocr->sequence_bound_receiver_
                    ->AsyncCall(&SequenceBoundReceiver::OnReceivedOCR)
                    .WithArgs(std::move(callback),
                              std::move(visual_annotation));
              },
              ref_ptr, std::move(callback)))));
}

void OpticalCharacterRecognizer::PerformOCR(
    const SkBitmap& image,
    base::OnceCallback<void(const ui::AXTreeUpdate&)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!is_ready()) {
    VLOG(0)
        << "PerformOCR called before the service is ready, returning empty.";
    std::move(callback).Run(ui::AXTreeUpdate());
    return;
  }

  MaybeConnectToOcrService();
  (*screen_ai_annotator_)
      ->PerformOcrAndReturnAXTreeUpdate(image, std::move(callback));
}

}  // namespace screen_ai
