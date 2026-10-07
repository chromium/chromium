// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SELECTION_WIDGET_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SELECTION_WIDGET_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/glic/browser_ui/glic_selection_widget.h"
#include "chrome/browser/glic/glic_selection_observer.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"

namespace content {
class WebContents;
}  // namespace content

enum class ToastId;

namespace glic {

class GlicKeyedService;
class GlicSelectionWidgetControllerDelegate;

class GlicSelectionWidgetController
    : public GlicSelectionWidgetDelegate::ActionDelegate,
      public content_settings::Observer {
 public:
  // TODO(b:570056510): Refactor the DismissReason out from the observer.
  using DismissReason = GlicSelectionObserver::DismissReason;

  enum class ShowResult {
    kShown,
    kNoBounds,
    kSkipped,
  };

  GlicSelectionWidgetController(
      content::WebContents* web_contents,
      GlicSelectionWidgetControllerDelegate& delegate);
  GlicSelectionWidgetController(const GlicSelectionWidgetController&) = delete;
  GlicSelectionWidgetController& operator=(
      const GlicSelectionWidgetController&) = delete;
  ~GlicSelectionWidgetController() override;

  // `GlicSelectionWidgetDelegate::ActionDelegate`:
  void OnAskGemini() override;
  void OnCopy() override;
  void OnHide() override;
  void OnSettings() override;
  void OnWidgetClose() override;
  gfx::Rect GetContainerBounds() override;

  // Dismisses the selection UI.
  // Virtual for testing.
  virtual void Dismiss(DismissReason reason);

  ShowResult Show(const std::u16string& selected_text);
  void Close();

  void OnPrimaryPageChanged();

 protected:
  // `content_settings::Observer`:
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

  // Shows the selection overlay.
  // Virtual for testing.
  virtual void ShowSelectionOverlay();

  // Returns true if the selection widget should be shown for the current page.
  bool ShouldShowSelectionWidget();

 private:
  void UpdatePageBlockedState();
  void ShowHiddenToast(ToastId toast_id);

  content::WebContents* web_contents() const { return web_contents_.get(); }

  base::WeakPtr<content::WebContents> web_contents_;
  const raw_ref<GlicSelectionWidgetControllerDelegate> delegate_;
  raw_ptr<GlicKeyedService> glic_keyed_service_;

  // True if a dismissal metric has already been recorded for the shown widget.
  bool dismissal_recorded_ = false;

  // True if the user temporarily blocked the selection widget for the current
  // page load.
  bool is_hidden_on_current_page_ = false;
  // True if the site is blocked from showing the inline cue by user settings or
  // default blocklist.
  bool is_site_blocked_on_current_page_ = false;

  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      content_settings_observation_{this};

  std::unique_ptr<GlicSelectionWidgetDelegate> widget_delegate_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SELECTION_WIDGET_CONTROLLER_H_
