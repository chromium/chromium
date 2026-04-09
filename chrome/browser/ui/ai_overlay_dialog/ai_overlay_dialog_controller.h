// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/class_property.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace views {
class WebView;
}  // namespace views

class HostContentSettingsMap;

namespace ttc {

class AiOverlayDialogController : public content::WebContentsDelegate {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnCaptionsVisibleChanged(bool visible) {}
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
  void ShowOverlay();

  // Hides the overlay.
  void HideOverlay();

  // Toggles the overlay visibility.
  void ToggleOverlay();

  bool IsOverlayShowing() const;

  // content::WebContentsDelegate:
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;

  bool captions_visible() const { return captions_visible_; }
  void set_captions_visible(bool visible);

  bool use_persona() const { return use_persona_; }
  void set_use_persona(bool use_persona);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  views::WebView* GetActiveOverlayWebView() const;

  raw_ptr<BrowserWindowInterface> browser_;

  ui::ScopedUnownedUserData<AiOverlayDialogController>
      scoped_unowned_user_data_;

  const raw_ptr<HostContentSettingsMap> host_content_settings_map_;

  bool captions_visible_ = true;
  bool use_persona_ = false;

  base::ObserverList<Observer> observers_;
};

extern const ::ui::ClassProperty<bool>* const kActionAiOverlayActiveKey;

}  // namespace ttc

#endif  // CHROME_BROWSER_UI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_CONTROLLER_H_
