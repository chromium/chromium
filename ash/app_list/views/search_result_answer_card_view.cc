// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_answer_card_view.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/content/public/cpp/navigable_contents.h"
#include "services/content/public/cpp/navigable_contents_view.h"
#include "services/content/public/mojom/navigable_contents_factory.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

constexpr char kSearchAnswerHasResult[] = "SearchAnswer-HasResult";
constexpr char kSearchAnswerIssuedQuery[] = "SearchAnswer-IssuedQuery";
constexpr char kSearchAnswerTitle[] = "SearchAnswer-Title";

// Selection color for answer card (3% black).
constexpr SkColor kAnswerCardSelectedColor =
    SkColorSetARGB(0x08, 0x00, 0x00, 0x00);

// Exclude the card native view from event handling.
void ExcludeCardFromEventHandling(gfx::NativeView card_native_view) {
  // |card_native_view| could be null in tests.
  if (!card_native_view)
    return;

  if (!card_native_view->parent()) {
    DLOG(ERROR) << "Card is not attached to the app list view.";
    return;
  }

  // |card_native_view| is brought into View's hierarchy via a NativeViewHost.
  // The window hierarchy looks like this:
  //   widget window -> clipping window -> content_native_view
  // Events should be targeted to the widget window and by-passing the sub tree
  // started at clipping window. Walking up the window hierarchy to find the
  // clipping window and make the cut there.
  aura::Window* top_level = card_native_view->GetToplevelWindow();
  DCHECK(top_level);
  aura::Window* window = card_native_view;
  while (window->parent() != top_level)
    window = window->parent();

  AppListView::ExcludeWindowFromEventHandling(window);
}

bool ParseResponseHeaders(const net::HttpResponseHeaders* headers,
                          std::string* title,
                          std::string* issued_query) {
  if (!headers || headers->response_code() != net::HTTP_OK)
    return false;

  if (!headers->HasHeaderValue(kSearchAnswerHasResult, "true")) {
    DLOG(WARNING) << "Response not an answer card. Expected a value of \"true\""
                  << " for " << kSearchAnswerHasResult << " header.";
    return false;
  }
  if (!headers->GetNormalizedHeader(kSearchAnswerTitle, title) ||
      title->empty()) {
    DLOG(WARNING) << "Ignoring answer card response with no valid "
                  << kSearchAnswerTitle << " header present.";
    return false;
  }
  if (!headers->GetNormalizedHeader(kSearchAnswerIssuedQuery, issued_query) ||
      issued_query->empty()) {
    DLOG(WARNING) << "Ignoring answer card response with no valid "
                  << kSearchAnswerIssuedQuery << " header present.";
    return false;
  }

  return true;
}

}  // namespace

// Answer Card Search Results that are contained within
// |SearchResultAnswerCardView|
class SearchResultAnswerCardView::AnswerCardResultView
    : public SearchResultBaseView,
      public content::NavigableContentsObserver {
 public:
  AnswerCardResultView(SearchResultContainerView* container,
                       AppListViewDelegate* view_delegate)
      : container_(container), view_delegate_(view_delegate) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetLayoutManager(std::make_unique<views::FillLayout>());

    view_delegate_->GetNavigableContentsFactory(
        contents_factory_.BindNewPipeAndPassReceiver());

    auto params = content::mojom::NavigableContentsParams::New();
    params->enable_view_auto_resize = true;
    params->suppress_navigations = true;
    params->override_background_color = true;
    params->background_color = SK_ColorTRANSPARENT;
    contents_ = std::make_unique<content::NavigableContents>(
        contents_factory_.get(), std::move(params));
    contents_->AddObserver(this);
  }

  ~AnswerCardResultView() override {
    contents_->RemoveObserver(this);
    ClearResult();
  }

  bool has_valid_answer_card() const {
    return is_current_navigation_valid_answer_card_;
  }

  void HideCard() {
    OnVisibilityChanged(false /* is_visible */);
    RemoveAllChildViews(false /* delete_children */);
    SetPreferredSize(gfx::Size{0, 0});

    // Force any future result changes to initiate another navigation.
    is_current_navigation_valid_answer_card_ = false;
  }

  void OnResultChanging(SearchResult* new_result) override {
    // Remove the card contents from the UI temporarily while we attempt to
    // navigate it to the new query URL.
    base::Optional<GURL> previous_url;
    if (result()) {
      previous_url = result()->query_url();
    }

    if (!new_result || !new_result->query_url()) {
      HideCard();
      return;
    }

    if (new_result->query_url() == previous_url &&
        is_current_navigation_valid_answer_card_) {
      // The new search result is for a query URL identical to the previous one,
      // so we don't bother hiding or navigating the existing card contents.
      return;
    }

    // We hide the view while navigating its contents. Once navigation is
    // finished and we (possibly) have a valid answer card response, the
    // contents view will be re-parented to this container.
    HideCard();

    base::RecordAction(base::UserMetricsAction("SearchAnswer_UserInteraction"));

    server_request_start_time_ = base::TimeTicks::Now();
    contents_->Navigate(*new_result->query_url());
  }

  void OnVisibilityChanged(bool is_visible) {
    if (is_visible && !last_shown_time_) {
      last_shown_time_ = base::Time::Now();
    } else if (last_shown_time_) {
      UMA_HISTOGRAM_MEDIUM_TIMES("SearchAnswer.AnswerVisibleTime",
                                 base::Time::Now() - *last_shown_time_);
      last_shown_time_.reset();
    }
  }

  // views::Button overrides:
  const char* GetClassName() const override { return "AnswerCardResultView"; }

  void OnBlur() override { SetSelected(false, base::nullopt); }

  void OnFocus() override {
    ScrollRectToVisible(GetLocalBounds());
    SetSelected(true, base::nullopt);
  }

  bool OnKeyPressed(const ui::KeyEvent& event) override {
    if (event.key_code() == ui::VKEY_SPACE) {
      // Shouldn't eat Space; we want Space to go to the search box.
      return false;
    }
    ActivateResult(event.flags(), false /* by_button_press */);
    return true;
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    // Default button role is atomic for ChromeVox, so assign a generic
    // container role to allow accessibility focus to get into this view.
    node_data->role = ax::mojom::Role::kGenericContainer;
    node_data->SetName(GetAccessibleName());
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    if (selected())
      canvas->FillRect(GetContentsBounds(), kAnswerCardSelectedColor);
  }

  // views::ButtonListener overrides:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    DCHECK(sender == this);
    ActivateResult(event.flags(), true /* by_button_press */);
  }

 private:
  void ActivateResult(int event_flags, bool by_button_press) {
    if (result()) {
      RecordSearchResultOpenSource(result(), view_delegate_->GetModel(),
                                   view_delegate_->GetSearchModel());
      view_delegate_->OpenSearchResult(
          result()->id(), event_flags,
          ash::AppListLaunchedFrom::kLaunchedFromSearchBox,
          ash::AppListLaunchType::kSearchResult, -1 /* suggestion_index */,
          !by_button_press && is_default_result() /* launch_as_default */);
    }
  }

  // content::NavigableContentsObserver overrides:
  void DidFinishNavigation(
      const GURL& url,
      bool is_main_frame,
      bool is_error_page,
      const net::HttpResponseHeaders* response_headers) override {
    if (!is_main_frame)
      return;

    is_current_navigation_valid_answer_card_ = false;
    if (is_error_page)
      return;

    std::string title;
    if (!ParseResponseHeaders(response_headers, &title, &answer_card_query_))
      return;

    SetAccessibleName(base::UTF8ToUTF16(title));

    UMA_HISTOGRAM_TIMES(
        "Apps.AppList.AnswerCardSearchProvider.SearchAnswerNavigationTime",
        base::TimeTicks::Now() - server_request_start_time_);

    is_current_navigation_valid_answer_card_ = true;
    answer_card_url_ = url;
  }

  void DidStopLoading() override {
    if (!is_current_navigation_valid_answer_card_)
      return;

    OnVisibilityChanged(true /* is_visible */);
    views::View* content_view = contents_->GetView()->view();
    if (children().empty()) {
      AddChildView(content_view);
      ExcludeCardFromEventHandling(contents_->GetView()->native_view());

      if (result() && result()->equivalent_result_id().has_value()) {
        view_delegate_->GetSearchModel()->DeleteResultById(
            result()->equivalent_result_id().value());
      }
    }
    SetPreferredSize(content_view->GetPreferredSize());
    container_->Update();

    UMA_HISTOGRAM_TIMES(
        "Apps.AppList.AnswerCardSearchProvider.SearchAnswerLoadingTime",
        base::TimeTicks::Now() - server_request_start_time_);
  }

  void DidAutoResizeView(const gfx::Size& new_size) override {
    SetPreferredSize(new_size);
    contents_->GetView()->view()->SetPreferredSize(new_size);
    container_->Update();
  }

  void DidSuppressNavigation(const GURL& url,
                             WindowOpenDisposition disposition,
                             bool from_user_gesture) override {
    if (!from_user_gesture)
      return;

    // NOTE: We shouldn't ever hit this path since all user gestures targeting
    // the content area should be intercepted by the Button which overlaps its
    // display region, and answer cards are generally not expected to elicit
    // scripted navigations. This action is recorded here to verify these
    // expectations.
    base::RecordAction(base::UserMetricsAction("SearchAnswer_OpenedUrl"));
  }

  void FocusedNodeChanged(bool is_editable_node,
                          const gfx::Rect& node_bounds_in_screen) override {}

  SearchResultContainerView* const container_;  // Not owned.
  AppListViewDelegate* const view_delegate_;    // Not owned.
  mojo::Remote<content::mojom::NavigableContentsFactory> contents_factory_;
  std::unique_ptr<content::NavigableContents> contents_;

  bool is_current_navigation_valid_answer_card_ = false;
  GURL answer_card_url_;
  std::string answer_card_query_;

  // Tracks the last time this view was made visible, if still visible.
  base::Optional<base::Time> last_shown_time_;

  base::TimeTicks server_request_start_time_;

  DISALLOW_COPY_AND_ASSIGN(AnswerCardResultView);
};

SearchResultAnswerCardView::SearchResultAnswerCardView(
    AppListViewDelegate* view_delegate)
    : SearchResultContainerView(view_delegate),
      search_answer_container_view_(
          new AnswerCardResultView(this, view_delegate)) {
  AddChildView(search_answer_container_view_);
  AddObservedResultView(search_answer_container_view_);
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

SearchResultAnswerCardView::~SearchResultAnswerCardView() = default;

const char* SearchResultAnswerCardView::GetClassName() const {
  return "SearchResultAnswerCardView";
}

int SearchResultAnswerCardView::GetYSize() {
  return num_results();
}

int SearchResultAnswerCardView::DoUpdate() {
  std::vector<SearchResult*> display_results =
      SearchModel::FilterSearchResultsByDisplayType(
          results(), ash::SearchResultDisplayType::kCard, /*excludes=*/{}, 1);
  SearchResult* top_result =
      display_results.empty() ? nullptr : display_results.front();

  const bool has_valid_answer_card =
      search_answer_container_view_->has_valid_answer_card();
  search_answer_container_view_->SetResult(top_result);
  parent()->SetVisible(has_valid_answer_card);

  set_container_score(
      has_valid_answer_card && top_result ? top_result->display_score() : 0);
  if (top_result)
    top_result->set_is_visible(has_valid_answer_card);

  return has_valid_answer_card ? 1 : 0;
}

bool SearchResultAnswerCardView::OnKeyPressed(const ui::KeyEvent& event) {
  if (search_answer_container_view_->OnKeyPressed(event))
    return true;

  return SearchResultContainerView::OnKeyPressed(event);
}

SearchResultBaseView* SearchResultAnswerCardView::GetFirstResultView() {
  return num_results() <= 0 ? nullptr : search_answer_container_view_;
}

SearchResultBaseView* SearchResultAnswerCardView::GetResultViewAt(
    size_t index) {
  DCHECK_EQ(index, 0u);
  return search_answer_container_view_;
}

views::View* SearchResultAnswerCardView::GetAnswerCardResultViewForTest()
    const {
  return search_answer_container_view_;
}

// static
scoped_refptr<net::HttpResponseHeaders>
SearchResultAnswerCardView::CreateAnswerCardResponseHeadersForTest(
    const std::string& query,
    const std::string& title) {
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  headers->AddHeader(base::StrCat({kSearchAnswerHasResult, ": true"}));
  headers->AddHeader(base::StrCat({kSearchAnswerTitle, ": ", title.c_str()}));
  headers->AddHeader(
      base::StrCat({kSearchAnswerIssuedQuery, ": ", query.c_str()}));
  return headers;
}

}  // namespace ash
