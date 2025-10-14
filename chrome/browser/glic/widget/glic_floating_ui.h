// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_FLOATING_UI_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_FLOATING_UI_H_

#include "base/time/time.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"
#include "chrome/browser/glic/widget/glic_window_event_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

class BrowserWindowInterface;

namespace glic {

class GlicWindowAnimator;
class GlicWidget;
class GlicView;

// A stub implementation of GlicUiEmbedder for floating UIs.
class GlicFloatingUi : public GlicUiEmbedder,
                       public Host::EmbedderDelegate,
                       public GlicWindowEventObserver::Delegate {
 public:
  GlicFloatingUi(Profile* profile,
                 BrowserWindowInterface* browser,
                 GlicUiEmbedder::Delegate& delegate);
  GlicFloatingUi(Profile* profile,
                 gfx::Rect initial_bounds,
                 GlicUiEmbedder::Delegate& delegate);
  ~GlicFloatingUi() override;

  static gfx::Size GetDefaultSize();

  // GlicUiEmbedder:
  Host::EmbedderDelegate* GetHostEmbedderDelegate() override;
  void Show() override;
  bool IsShowing() const override;
  void Close() override;
  std::unique_ptr<GlicUiEmbedder> CreateInactiveEmbedder() const override;
  void Focus() override;
  views::View* GetView() override;
  mojom::PanelState GetPanelState() const override;
  gfx::Size GetPanelSize() override;

  // Host::EmbedderDelegate:
  void Resize(const gfx::Size& size,
              base::TimeDelta duration,
              base::OnceClosure callback) override;
  void SetDraggableAreas(
      const std::vector<gfx::Rect>& draggable_areas) override;
  void EnableDragResize(bool enabled) override;
  void Attach() override;
  void Detach() override;
  void SetMinimumWidgetSize(const gfx::Size& size) override;
  void SwitchConversation(
      glic::mojom::ConversationInfoPtr info,
      mojom::WebClientHandler::SwitchConversationCallback callback) override;
  void ClosePanel() override;

  // GlicWindowEventObserver::Delegate:
  GlicWindowAnimator* window_animator() override;
  void OnDragComplete() override;

 private:
  GlicWidget* GetGlicWidget() const;
  GlicView* GetGlicView() const;
  void CreateAndSetupWidget(gfx::Rect initial_bounds);
  void MaybeSetWidgetCanResize();
  void SetGlicWindowToFloatingMode(bool floating);

  // Whether the widget should be user resizable, kept here in case it's
  // specified before the widget is created.
  bool user_resizable_ = true;

  std::unique_ptr<GlicWindowAnimator> glic_window_animator_;
  std::unique_ptr<GlicWidget> glic_widget_;
  std::unique_ptr<GlicWindowEventObserver> window_event_observer_;
  mojom::PanelState panel_state_;

  raw_ptr<Profile> profile_;
  raw_ref<GlicUiEmbedder::Delegate> delegate_;

  base::WeakPtrFactory<GlicFloatingUi> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_FLOATING_UI_H_
