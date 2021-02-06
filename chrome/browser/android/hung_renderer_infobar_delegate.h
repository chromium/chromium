// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_HUNG_RENDERER_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_HUNG_RENDERER_INFOBAR_DELEGATE_H_

#include "base/macros.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace content {
class RenderProcessHost;
}

class InfoBarService;

// A hung renderer infobar is shown when the when the renderer is deemed
// unresponsive. The infobar provides the user with a choice of either
// waiting for the renderer to regain responsiveness, or killing the
// renderer immediately. This class provides the resources necessary to
// display such an infobar, also logging the action taken by the user
// (if any) for UMA purposes.
class HungRendererInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a hung renderer InfoBar, adding it to the provided
  // |infobar_service|. The |render_process_host| will be used to kill the
  // renderer process if the user so chooses.
  static void Create(InfoBarService* infobar_service,
                     content::RenderProcessHost* render_process_host);

  // Called if the renderer regains responsiveness before the infobar is
  // dismissed.
  void OnRendererResponsive();

 private:
  // Keep these values in alignment with their histograms.xml counterparts.
  enum Event {
    WAIT_CLICKED = 0,
    KILL_CLICKED,
    CLOSE_CLICKED,
    RENDERER_BECAME_RESPONSIVE,
    TAB_CLOSED,
    EVENT_COUNT
  };

  explicit HungRendererInfoBarDelegate(
      content::RenderProcessHost* render_process_host);
  ~HungRendererInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  void InfoBarDismissed() override;
  HungRendererInfoBarDelegate* AsHungRendererInfoBarDelegate() override;
  int GetIconId() const override;
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  void LogEvent(Event event);

  // Used to terminate the renderer process if the user clicks the kill button.
  content::RenderProcessHost* render_process_host_;

  bool terminal_event_logged_for_uma_;

  DISALLOW_COPY_AND_ASSIGN(HungRendererInfoBarDelegate);
};

#endif  // CHROME_BROWSER_ANDROID_HUNG_RENDERER_INFOBAR_DELEGATE_H_
