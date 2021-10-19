// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ICON_VIEW_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ICON_VIEW_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/image_view.h"

namespace ash {

// A class for loading and displaying the icon of apps/urls used in a
// DesksTemplatesItemView.
class DesksTemplatesIconView : public views::ImageView {
 public:
  METADATA_HEADER(DesksTemplatesIconView);

  DesksTemplatesIconView();
  DesksTemplatesIconView(const DesksTemplatesIconView&) = delete;
  DesksTemplatesIconView& operator=(const DesksTemplatesIconView&) = delete;
  ~DesksTemplatesIconView() override;

  // The size of an icon.
  static constexpr int kIconSize = 28;

  void SetIconIdentifier(const std::string& icon_identifier) {
    icon_identifier_ = icon_identifier;
  }
  void SetIsUrl(bool is_url) { is_url_ = is_url; }

  // Fetches the icon for the provided `icon_identifier_`.
  void LoadIcon();

 private:
  // Callbacks for when the app icon/favicon has been fetched. If the result is
  // non-null/empty then we'll set this's image to the result. Otherwise, we'll
  // use a placeholder icon.
  void OnFaviconLoaded(const favicon_base::FaviconImageResult& image_result);
  void OnAppIconLoaded(apps::mojom::IconValuePtr icon_value);

  // The identifier for an icon. For a favicon, this will be a url. For an app,
  // this will be an app id.
  std::string icon_identifier_;

  // If true, `icon_identifier_` is a url. Otherwise, `icon_identifier_` is an
  // app id.
  bool is_url_ = false;

  // Used for favicon loading tasks.
  base::CancelableTaskTracker cancelable_task_tracker_;

  base::WeakPtrFactory<DesksTemplatesIconView> weak_ptr_factory_{this};
};

BEGIN_VIEW_BUILDER(/* no export */, DesksTemplatesIconView, views::ImageView)
VIEW_BUILDER_PROPERTY(std::string, IconIdentifier)
VIEW_BUILDER_PROPERTY(bool, IsUrl)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::DesksTemplatesIconView)

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ICON_VIEW_H_
