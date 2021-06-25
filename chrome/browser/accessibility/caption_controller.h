// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_CAPTION_CONTROLLER_H_
#define CHROME_BROWSER_ACCESSIBILITY_CAPTION_CONTROLLER_H_

#include <memory>
#include <unordered_map>

#include "chrome/browser/accessibility/soda_installer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/common/caption.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "ui/native_theme/caption_style.h"
#include "ui/native_theme/native_theme_observer.h"

class Browser;
class Profile;
class PrefChangeRegistrar;

namespace ui {
class NativeTheme;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace captions {

class CaptionBubbleController;
class CaptionHostImpl;

///////////////////////////////////////////////////////////////////////////////
// Caption Controller
//
//  The controller of the live caption feature. It enables the captioning
//  service when the preference is enabled. The caption controller is a
//  KeyedService and BrowserListObserver. There exists one caption controller
//  per profile and it lasts for the duration of the session. The caption
//  controller owns the live caption UI, which are caption bubble controllers.
//
class CaptionController : public BrowserListObserver,
                          public KeyedService,
                          public speech::SodaInstaller::Observer,
                          public ui::NativeThemeObserver {
 public:
  explicit CaptionController(Profile* profile);
  ~CaptionController() override;
  CaptionController(const CaptionController&) = delete;
  CaptionController& operator=(const CaptionController&) = delete;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  void Init();

  // Routes a transcription to the CaptionBubbleController that belongs to the
  // appropriate browser. Returns whether the transcription result was routed
  // successfully. Transcriptions will halt if this returns false.
  bool DispatchTranscription(
      CaptionHostImpl* caption_host_impl,
      const chrome::mojom::TranscriptionResultPtr& transcription_result);

  void OnLanguageIdentificationEvent(
      const media::mojom::LanguageIdentificationEventPtr& event);

  // Alerts the CaptionBubbleController that belongs to the appropriate browser
  // that there is an error in the speech recognition service.
  void OnError(CaptionHostImpl* caption_host_impl);

  // Alerts the CaptionBubbleController that belongs to the appropriate browser
  // that the audio stream has ended.
  void OnAudioStreamEnd(CaptionHostImpl* caption_host_impl);

  CaptionBubbleController* GetCaptionBubbleControllerForBrowser(
      Browser* browser);

 private:
  friend class CaptionControllerFactory;
  friend class CaptionControllerTest;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // SodaInstaller::Observer:
  void OnSodaInstalled() override;
  void OnSodaProgress(int progress) override {}
  void OnSodaError() override {}

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

  // Owns us via the KeyedService mechanism.
  Profile* profile_;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // A map of Browsers using this profile to CaptionBubbleControllers anchored
  // to the browser.
  std::unordered_map<Browser*, std::unique_ptr<CaptionBubbleController>>
      caption_bubble_controllers_;

  base::Optional<ui::CaptionStyle> caption_style_;

  // Whether Live Caption is enabled.
  bool enabled_ = false;

  // Whether the UI has been created. The UI is created asynchronously from the
  // feature being enabled--we wait for SODA to download first. This flag
  // ensures that the UI is not constructed or deconstructed twice.
  bool is_ui_constructed_ = false;
};

}  // namespace captions

#endif  // CHROME_BROWSER_ACCESSIBILITY_CAPTION_CONTROLLER_H_
