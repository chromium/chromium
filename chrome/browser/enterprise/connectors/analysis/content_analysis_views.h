// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_VIEWS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_VIEWS_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/throbber.h"

namespace enterprise_connectors {

// Common interface shared by other classes in this file to access information
// about their shared content analysis dialog.
class ContentAnalysisBaseView {
 public:
  class Delegate {
   public:
    // Returns the appropriate top image ID depending on the current state of
    // the content analysis dialog.
    virtual int GetTopImageId() const = 0;

    // Returns the color to be used to render the content analysis dialog's side
    // logo.
    virtual ui::ColorId GetSideImageLogoColor() const = 0;

    // Returns the side image's background circle color depending on the state
    // of the content analysis dialog.
    virtual ui::ColorId GetSideImageBackgroundColor() const = 0;

    // Returns true if the dialog is showing a non-pending state representing
    // the final result of the content analysis.
    virtual bool is_result() const = 0;
  };

  explicit ContentAnalysisBaseView(Delegate* delegate);
  Delegate* delegate();

 private:
  raw_ptr<Delegate, DanglingUntriaged> delegate_;
};

// `views::ImageView` representing the image at the top of the content analysis
// dialog.
class ContentAnalysisTopImageView : public ContentAnalysisBaseView,
                                    public views::ImageView {
  METADATA_HEADER(ContentAnalysisTopImageView, views::ImageView)
 public:
  using ContentAnalysisBaseView::ContentAnalysisBaseView;

  void Update();

 protected:
  void OnThemeChanged() override;
};

// `views::ImageView` representing the enterprise building logo shown on the
// left side of the content analysis dialog.
class ContentAnalysisSideIconImageView : public ContentAnalysisBaseView,
                                         public views::ImageView {
  METADATA_HEADER(ContentAnalysisSideIconImageView, views::ImageView)

 public:
  explicit ContentAnalysisSideIconImageView(Delegate* delegate);

  void Update();

 protected:
  void OnThemeChanged() override;
};

// `views::Throbber` shown on the left of the content analysis dialog while the
// user is waiting for a verdict.
class ContentAnalysisSideIconSpinnerView : public ContentAnalysisBaseView,
                                           public views::Throbber {
  METADATA_HEADER(ContentAnalysisSideIconSpinnerView, views::Throbber)

 public:
  using ContentAnalysisBaseView::ContentAnalysisBaseView;

  void Update();

 protected:
  void OnThemeChanged() override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_VIEWS_H_
