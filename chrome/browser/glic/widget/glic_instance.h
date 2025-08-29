// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_INSTANCE_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_INSTANCE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/glic_zero_state_suggestions_manager.h"
#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/glic_ui_embedder.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/widget/panel_delegate.h"

class BrowserWindowInterface;

namespace glic {

// A Panel owns a single host keeping any state that must exist for the lifetime
// of the host. When a host is showing the Panel creates a PanelEmbedderDelegate
// to display the webcontents in. A panel (and host) exist even if it has no
// PanelEmbedderDelegate showing the panel. A host could have many different
// PanelEmbedderDelegates during its lifetime.
class GlicInstance : public PanelDelegate {
 public:
  class AttachmentDelegate {
   public:
    virtual ~AttachmentDelegate() = default;
    virtual void AttachInstance(GlicInstance* instance) = 0;
    virtual void DetachInstance(GlicInstance* instance) = 0;
  };

  explicit GlicInstance(base::WeakPtr<AttachmentDelegate> attachment_delegate);
  ~GlicInstance() override;

  GlicInstance(const GlicInstance&) = delete;
  GlicInstance& operator=(const GlicInstance&) = delete;

  void AttachPanel() override;
  void DetachPanel() override;
  bool IsShowing() const;

  // PanelDelegate:
  void ClosePanelAndShutdown() override;
  void CreateTab() override;
  void CreateTask() override;
  void PerformActions() override;
  void StopActorTask() override;
  void PauseActorTask() override;
  void ResumeActorTask() override;
  void GetZeroStateSuggestionsAndSubscribe() override;
  void GetZeroStateSuggestionsForFocusedTab() override;

 private:
  // Objects that currently live in GlicService moved to here.
  std::unique_ptr<glic::Host> host_;
  std::unique_ptr<GlicScreenshotCapturer> screenshot_capturer_;
  std::unique_ptr<GlicSharingManagerImpl> sharing_manager_;
  std::unique_ptr<GlicZeroStateSuggestionsManager>
      zero_state_suggestions_manager_;

  // Replaces GlicWindowController on existing GlicKeyedService.
  std::unique_ptr<GlicUiEmbedder> embedder_;
  // bool is_chat_mode = true;
  // Probably initiated from a default setting but can be changed independently
  // per a Panel.
  // bool is_following_active_tab_ = true;

  // The attached browser if currently showing in the side panel of a window.
  raw_ptr<BrowserWindowInterface> attached_bwi_ = nullptr;
  base::WeakPtr<AttachmentDelegate> attachment_delegate_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_INSTANCE_H_
