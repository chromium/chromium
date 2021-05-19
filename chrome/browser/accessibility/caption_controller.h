// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_CAPTION_CONTROLLER_H_
#define CHROME_BROWSER_ACCESSIBILITY_CAPTION_CONTROLLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "ui/native_theme/caption_style.h"
#include "ui/native_theme/native_theme_observer.h"

class PrefChangeRegistrar;

namespace ui {
class NativeTheme;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace captions {

class CaptionBubbleController;
class LiveCaptionSpeechRecognitionHost;

///////////////////////////////////////////////////////////////////////////////
// Caption Controller
//
//  The controller of the live caption feature. It enables the captioning
//  service when the preference is enabled. The caption controller is a
//  KeyedService. There exists one caption controller per profile and it lasts
//  for the duration of the session. The caption controller owns the live
//  caption UI, which is a caption bubble controller.
//
class CaptionController : public KeyedService,
                          public speech::SodaInstaller::Observer,
                          public ui::NativeThemeObserver {
 public:
  explicit CaptionController(PrefService* profile_prefs);
  ~CaptionController() override;
  CaptionController(const CaptionController&) = delete;
  CaptionController& operator=(const CaptionController&) = delete;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  void Init();

  // Routes a transcription to the CaptionBubbleController. Returns whether the
  // transcription result was routed successfully. Transcriptions will halt if
  // this returns false.
  bool DispatchTranscription(
      LiveCaptionSpeechRecognitionHost* live_caption_speech_recognition_host,
      const media::mojom::SpeechRecognitionResultPtr& result);

  void OnLanguageIdentificationEvent(
      const media::mojom::LanguageIdentificationEventPtr& event);

  // Alerts the CaptionBubbleController that there is an error in the speech
  // recognition service.
  void OnError(
      LiveCaptionSpeechRecognitionHost* live_caption_speech_recognition_host);

  // Alerts the CaptionBubbleController that the audio stream has ended.
  void OnAudioStreamEnd(
      LiveCaptionSpeechRecognitionHost* live_caption_speech_recognition_host);

 private:
  friend class CaptionControllerFactory;
  friend class CaptionControllerTest;

  // SodaInstaller::Observer:
  void OnSodaInstalled() override;
  void OnSodaLanguagePackInstalled(
      speech::LanguageCode language_code) override {}
  void OnSodaProgress(int combined_progress) override {}
  void OnSodaLanguagePackProgress(int language_progress,
                                  speech::LanguageCode language_code) override {
  }
  void OnSodaError() override {}
  void OnSodaLanguagePackError(speech::LanguageCode language_code) override {}

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override {}
  void OnCaptionStyleUpdated() override;

  void OnLiveCaptionEnabledChanged();
  void OnLiveCaptionLanguageChanged();
  bool IsLiveCaptionEnabled();
  void StartLiveCaption();
  void StopLiveCaption();
  void CreateUI();
  void DestroyUI();

  void UpdateAccessibilityCaptionHistograms();

  PrefService* profile_prefs_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  std::unique_ptr<CaptionBubbleController> caption_bubble_controller_;
  absl::optional<ui::CaptionStyle> caption_style_;

  // Whether Live Caption is enabled.
  bool enabled_ = false;

  // Whether the UI has been created. The UI is created asynchronously from the
  // feature being enabled--we wait for SODA to download first. This flag
  // ensures that the UI is not constructed or deconstructed twice.
  bool is_ui_constructed_ = false;

  base::WeakPtrFactory<CaptionController> weak_ptr_factory_{this};
};

}  // namespace captions

#endif  // CHROME_BROWSER_ACCESSIBILITY_CAPTION_CONTROLLER_H_
