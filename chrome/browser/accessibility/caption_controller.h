// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_CAPTION_CONTROLLER_H_
#define CHROME_BROWSER_ACCESSIBILITY_CAPTION_CONTROLLER_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/common/caption.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/native_theme/caption_style.h"

class Browser;
class Profile;
class PrefChangeRegistrar;

namespace content {
class WebContents;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace captions {

class CaptionBubbleController;

///////////////////////////////////////////////////////////////////////////////
// Caption Controller
//
//  The controller of the live caption feature. It enables the captioning
//  service when the preference is enabled. The caption controller is a
//  KeyedService and BrowserListObserver. There exists one caption controller
//  per profile and it lasts for the duration of the session. The caption
//  controller owns the live caption UI, which are caption bubble controllers.
//
class CaptionController : public BrowserListObserver, public KeyedService {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. These should be the same as
  // LiveCaptionSessionEvent in enums.xml.
  enum class SessionEvent {
    // We began receiving captions for an audio stream.
    kStreamStarted = 0,
    // The audio stream ended, meaning no more captions will be received on that
    // stream.
    kStreamEnded = 1,
    // The close button was clicked, so we stopped listening to an audio stream.
    kCloseButtonClicked = 2,
    kMaxValue = kCloseButtonClicked,
  };

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
      content::WebContents* web_contents,
      const chrome::mojom::TranscriptionResultPtr& transcription_result);

  // Alerts the CaptionBubbleController that belongs to the appropriate browser
  // that there is an error in the speech recognition service.
  void OnError(content::WebContents* web_contents);

  CaptionBubbleController* GetCaptionBubbleControllerForBrowser(
      Browser* browser);

 private:
  friend class CaptionControllerFactory;
  friend class CaptionControllerTest;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  void OnLiveCaptionEnabledChanged();
  void OnLiveCaptionLanguageChanged();
  bool IsLiveCaptionEnabled();
  void UpdateSpeechRecognitionServiceEnabled();
  void UpdateSpeechRecognitionLanguage();
  void UpdateUIEnabled();
  void UpdateCaptionStyle();

  void UpdateAccessibilityCaptionHistograms();

  // Owns us via the KeyedService mechanism.
  Profile* profile_;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // A map of Browsers using this profile to CaptionBubbleControllers anchored
  // to the browser.
  std::unordered_map<Browser*, std::unique_ptr<CaptionBubbleController>>
      caption_bubble_controllers_;

  base::Optional<ui::CaptionStyle> caption_style_;

  bool enabled_ = false;
};

}  // namespace captions

#endif  // CHROME_BROWSER_ACCESSIBILITY_CAPTION_CONTROLLER_H_
