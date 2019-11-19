// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_ANSWER_CARD_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_ANSWER_CARD_VIEW_H_

#include "ash/app_list/views/search_result_container_view.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "net/http/http_response_headers.h"

namespace ash {

class AppListViewDelegate;

// Result container for the search answer card.
class APP_LIST_EXPORT SearchResultAnswerCardView
    : public SearchResultContainerView {
 public:
  explicit SearchResultAnswerCardView(AppListViewDelegate* view_delegate);
  ~SearchResultAnswerCardView() override;

  // Overridden from views::View:
  const char* GetClassName() const override;

  // Overridden from SearchResultContainerView:
  void NotifyFirstResultYIndex(int y_index) override {}
  int GetYSize() override;
  int DoUpdate() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  SearchResultBaseView* GetFirstResultView() override;
  SearchResultBaseView* GetResultViewAt(size_t index) override;

  views::View* GetAnswerCardResultViewForTest() const;

  static scoped_refptr<net::HttpResponseHeaders>
  CreateAnswerCardResponseHeadersForTest(const std::string& query,
                                         const std::string& title);

 private:
  class AnswerCardResultView;

  // Pointer to the container of the search answer; owned by the view hierarchy.
  // It's visible iff we have a search answer result.
  AnswerCardResultView* const search_answer_container_view_;

  // Tracks the last known card title so we can update the accessibility
  // framework if the title changes while the card has focus.
  base::string16 last_known_card_title_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultAnswerCardView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_ANSWER_CARD_VIEW_H_
