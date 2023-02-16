// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_video_view.h"

#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/ash_web_view_factory.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "net/base/url_util.h"
#include "ui/views/layout/fill_layout.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace ash {

namespace {

constexpr base::StringPiece kAmbientVideoSrcQueryParam = "video_src";

GURL BuildFileUrl(const base::FilePath& file_path) {
  return GURL(base::StrCat(
      {url::kFileScheme, url::kStandardSchemeSeparator, file_path.value()}));
}

}  // namespace

AmbientVideoView::AmbientVideoView(const base::FilePath& video_path,
                                   const base::FilePath& html_path) {
  DCHECK(!video_path.empty());
  DCHECK(!html_path.empty());
  DCHECK(AshWebViewFactory::Get());
  SetUseDefaultFillLayout(true);
  AshWebView* ash_web_view =
      AddChildView(AshWebViewFactory::Get()->Create(AshWebView::InitParams()));
  ash_web_view->SetID(kAmbientVideoWebView);
  ash_web_view->SetUseDefaultFillLayout(true);
  GURL ambient_video_url = net::AppendQueryParameter(
      BuildFileUrl(html_path), kAmbientVideoSrcQueryParam,
      BuildFileUrl(video_path).spec());
  ash_web_view->Navigate(ambient_video_url);
}

AmbientVideoView::~AmbientVideoView() = default;

}  // namespace ash
