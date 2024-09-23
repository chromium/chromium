// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/annotator/annotations_overlay_view_impl.h"

#include <memory>

#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "url/gurl.h"

AnnotationsOverlayViewImpl::AnnotationsOverlayViewImpl(
    content::BrowserContext* browser_context)
    : web_view_(
          AddChildView(std::make_unique<views::WebView>(browser_context))) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Use a transparent background for the web contents.
  auto* web_contents = web_view_->GetWebContents();
  DCHECK(web_contents);
  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      web_contents, SK_ColorTRANSPARENT);

  // Loading the annotator app in `web_view_` can take a long time, so in order
  // to avoid stalling the initialization of recording, we will do this
  // asynchronously.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&AnnotationsOverlayViewImpl::InitializeAnnotator,
                     weak_ptr_factory_.GetWeakPtr()));
}

AnnotationsOverlayViewImpl::~AnnotationsOverlayViewImpl() = default;

void AnnotationsOverlayViewImpl::InitializeAnnotator() {
  web_view_->LoadInitialURL(GURL(ash::kChromeUIUntrustedAnnotatorUrl));
}

BEGIN_METADATA(AnnotationsOverlayViewImpl)
END_METADATA
