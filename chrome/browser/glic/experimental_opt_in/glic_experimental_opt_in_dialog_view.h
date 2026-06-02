// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_DIALOG_VIEW_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_DIALOG_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/view_observer.h"
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
                                        public views::ViewObserver {
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

  // views::ViewObserver:
  void OnViewAddedToWidget(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

 private:
  raw_ptr<views::WebView> web_view_ = nullptr;
  base::ScopedObservation<views::View, views::ViewObserver> view_observation_{
      this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_DIALOG_VIEW_H_
