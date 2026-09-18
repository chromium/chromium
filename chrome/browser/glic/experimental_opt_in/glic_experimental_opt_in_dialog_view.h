// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_DIALOG_VIEW_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_DIALOG_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

class Profile;

namespace views {
class WebView;
}

namespace tabs {
class TabInterface;
}

namespace glic {

class GlicExperimentalOptInDialogView : public views::DialogDelegate,
                                        public views::ViewObserver,
                                        public views::WidgetObserver,
                                        public content::WebContentsObserver {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDialogElementId);

  explicit GlicExperimentalOptInDialogView(Profile* profile,
                                           tabs::TabInterface* tab_interface);

  GlicExperimentalOptInDialogView(const GlicExperimentalOptInDialogView&) =
      delete;
  GlicExperimentalOptInDialogView& operator=(
      const GlicExperimentalOptInDialogView&) = delete;
  ~GlicExperimentalOptInDialogView() override;

  views::WebView* GetWebViewForTesting();

  // content::WebContentsObserver:
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

  // views::ViewObserver:
  void OnViewAddedToWidget(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  // Announces the dialog's accessible title, at most once per dialog. Does
  // nothing if the widget is no longer visible: this dialog is tab-modal, so
  // it can be hidden (e.g. the user switches tabs) between the time the
  // announcement is posted and the time it runs, and announcing a title for a
  // dialog the user cannot see would be misleading.
  void AnnounceAccessibleTitle();

  raw_ptr<views::WebView> web_view_ = nullptr;
  bool title_announced_ = false;
  base::ScopedObservation<views::View, views::ViewObserver> view_observation_{
      this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  base::WeakPtrFactory<GlicExperimentalOptInDialogView> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_DIALOG_VIEW_H_
