// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_SEARCH_RESULTS_PANEL_H_
#define ASH_CAPTURE_MODE_SEARCH_RESULTS_PANEL_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/widget/widget.h"

namespace ash {

class AshWebView;

// Container for the search results view and other UI such as the search box,
// close button, etc.
class ASH_EXPORT SearchResultsPanel : public views::View {
  METADATA_HEADER(SearchResultsPanel, views::View)

 public:
  SearchResultsPanel();
  SearchResultsPanel(const SearchResultsPanel&) = delete;
  SearchResultsPanel& operator=(const SearchResultsPanel&) = delete;
  ~SearchResultsPanel() override;

  // Currently only used in tests.
  static std::unique_ptr<views::Widget> CreateWidget();

  AshWebView* search_results_view() const { return search_results_view_; }

 private:
  // Owned by the views hierarchy.
  raw_ptr<AshWebView> search_results_view_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_SEARCH_RESULTS_PANEL_H_
