// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_CONTROLLER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/class_property.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class HostContentSettingsMap;

namespace ttc {

namespace prefs {
inline constexpr char kAiOverlayRememberedNotes[] =
    "ai_overlay.remembered_notes";
}  // namespace prefs

class AiOverlayDialogController : public content::WebContentsDelegate {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnInputCaptionsVisibleChanged(bool visible) {}
    virtual void OnOutputCaptionsVisibleChanged(bool visible) {}
    virtual void OnUsePersonaChanged(bool use_persona) {}
  };

  DECLARE_USER_DATA(AiOverlayDialogController);

  static AiOverlayDialogController* From(BrowserWindowInterface* browser);

  explicit AiOverlayDialogController(BrowserWindowInterface* browser);
  AiOverlayDialogController(const AiOverlayDialogController&) = delete;
  AiOverlayDialogController& operator=(const AiOverlayDialogController&) =
      delete;
  ~AiOverlayDialogController() override;


  // Shows the transparent overlay above the browser window.
  virtual void ShowOverlay() = 0;

  // Hides the overlay.
  virtual void HideOverlay() = 0;

  // Toggles the overlay visibility.
  void ToggleOverlay();

  virtual bool IsOverlayShowing() const = 0;

  // content::WebContentsDelegate:
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  bool input_captions_visible() const { return input_captions_visible_; }
  void SetInputCaptionsVisible(bool visible);

  bool output_captions_visible() const { return output_captions_visible_; }
  void SetOutputCaptionsVisible(bool visible);

  bool captions_visible() const {
    return input_captions_visible_ && output_captions_visible_;
  }
  void SetCaptionsVisible(bool visible);

  bool use_persona() const { return use_persona_; }
  void SetUsePersona(bool use_persona);

  std::vector<std::pair<std::string, std::string>> GetRememberedNotes() const;
  void SetRememberedNote(const std::string& key, const std::string& value);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  BrowserWindowInterface* browser() const { return browser_; }

 private:
  raw_ptr<BrowserWindowInterface> browser_;

  ui::ScopedUnownedUserData<AiOverlayDialogController>
      scoped_unowned_user_data_;

  const raw_ptr<HostContentSettingsMap> host_content_settings_map_;

  bool input_captions_visible_ = true;
  bool output_captions_visible_ = true;
  bool use_persona_ = false;

  base::ObserverList<Observer> observers_;
};

extern const ::ui::ClassProperty<bool>* const kActionAiOverlayActiveKey;

}  // namespace ttc

#endif  // CHROME_BROWSER_UI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_CONTROLLER_H_
