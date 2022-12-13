// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_MAIN_VIEW_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_MAIN_VIEW_H_

#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/views/search_box_view_delegate.h"
#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/views/view.h"

namespace ash {

class AppListView;
class AppListViewDelegate;
class ApplicationDragAndDropHost;
class ContentsView;
class PaginationModel;
class SearchBoxView;
class SearchBoxViewBase;

// AppListMainView contains the normal view of the app list, which is shown
// when the user is signed in.
class ASH_EXPORT AppListMainView : public views::View,
                                   public SearchBoxViewDelegate {
 public:
  AppListMainView(AppListViewDelegate* delegate, AppListView* app_list_view);

  AppListMainView(const AppListMainView&) = delete;
  AppListMainView& operator=(const AppListMainView&) = delete;

  ~AppListMainView() override;

  void Init(int initial_apps_page, SearchBoxView* search_box_view);

  void ShowAppListWhenReady();

  SearchBoxView* search_box_view() const { return search_box_view_; }

  // If |drag_and_drop_host| is not nullptr it will be called upon drag and drop
  // operations outside the application list.
  void SetDragAndDropHostOfCurrentAppList(
      ApplicationDragAndDropHost* drag_and_drop_host);

  ContentsView* contents_view() const { return contents_view_; }
  AppListViewDelegate* view_delegate() { return delegate_; }

  // Overridden from views::View:
  const char* GetClassName() const override;
  void Layout() override;

 private:
  // Adds the ContentsView.
  void AddContentsViews();

  // Gets the PaginationModel owned by the AppsGridView.
  PaginationModel* GetAppsPaginationModel();

  // Overridden from SearchBoxViewDelegate:
  void QueryChanged(const std::u16string& trimmed_query,
                    bool initiated_by_user) override;
  void AssistantButtonPressed() override;
  void CloseButtonPressed() override;
  void ActiveChanged(SearchBoxViewBase* sender) override;
  void OnSearchBoxKeyEvent(ui::KeyEvent* event) override;
  bool CanSelectSearchResults() override;

  AppListViewDelegate* delegate_;  // Owned by parent view (AppListView).

  // Created by AppListView. Owned by views hierarchy.
  SearchBoxView* search_box_view_ = nullptr;

  ContentsView* contents_view_ = nullptr;  // Owned by views hierarchy.
  AppListView* const app_list_view_;       // Owned by views hierarchy.
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_MAIN_VIEW_H_
