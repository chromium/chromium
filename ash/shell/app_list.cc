// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_item_list.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/search/search_box_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell/example_factory.h"
#include "ash/shell/toplevel_window.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/string_search.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/examples_window.h"

namespace ash {
namespace shell {

namespace {

// WindowTypeShelfItem is an app item of app list. It carries a window
// launch type and launches corresponding example window when activated.
class WindowTypeShelfItem : public app_list::AppListItem {
 public:
  enum Type {
    TOPLEVEL_WINDOW = 0,
    NON_RESIZABLE_WINDOW,
    LOCK_SCREEN,
    WIDGETS_WINDOW,
    EXAMPLES_WINDOW,
    LAST_TYPE,
  };

  WindowTypeShelfItem(const std::string& id, Type type);
  ~WindowTypeShelfItem() override;

  static gfx::ImageSkia GetIcon(Type type) {
    static const SkColor kColors[] = {
        SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE, SK_ColorYELLOW, SK_ColorCYAN,
    };

    const int kIconSize = 128;
    SkBitmap icon;
    icon.allocN32Pixels(kIconSize, kIconSize);
    icon.eraseColor(kColors[static_cast<int>(type) % arraysize(kColors)]);
    return gfx::ImageSkia::CreateFrom1xBitmap(icon);
  }

  // The text below is not localized as this is an example code.
  static std::string GetTitle(Type type) {
    switch (type) {
      case TOPLEVEL_WINDOW:
        return "Create Window";
      case NON_RESIZABLE_WINDOW:
        return "Create Non-Resizable Window";
      case LOCK_SCREEN:
        return "Lock Screen";
      case WIDGETS_WINDOW:
        return "Show Example Widgets";
      case EXAMPLES_WINDOW:
        return "Open Views Examples Window";
      default:
        return "Unknown window type.";
    }
  }

  // The text below is not localized as this is an example code.
  static std::string GetDetails(Type type) {
    // Assigns details only to some types so that we see both one-line
    // and two-line results.
    switch (type) {
      case WIDGETS_WINDOW:
        return "Creates a window to show example widgets";
      case EXAMPLES_WINDOW:
        return "Creates a window to show views example.";
      default:
        return std::string();
    }
  }

  static void ActivateItem(Type type, int event_flags) {
    switch (type) {
      case TOPLEVEL_WINDOW: {
        ToplevelWindow::CreateParams params;
        params.can_resize = true;
        ToplevelWindow::CreateToplevelWindow(params);
        break;
      }
      case NON_RESIZABLE_WINDOW: {
        ToplevelWindow::CreateToplevelWindow(ToplevelWindow::CreateParams());
        break;
      }
      case LOCK_SCREEN: {
        Shell::Get()->session_controller()->LockScreen();
        break;
      }
      case WIDGETS_WINDOW: {
        CreateWidgetsWindow();
        break;
      }
      case EXAMPLES_WINDOW: {
        views::examples::ShowExamplesWindow(base::DoNothing());
        break;
      }
      default:
        break;
    }
  }

  void Activate(int event_flags) { ActivateItem(type_, event_flags); }

 private:
  Type type_;

  DISALLOW_COPY_AND_ASSIGN(WindowTypeShelfItem);
};

WindowTypeShelfItem::WindowTypeShelfItem(const std::string& id, Type type)
    : app_list::AppListItem(id), type_(type) {
  std::string title(GetTitle(type));
  SetIcon(GetIcon(type));
  SetName(title);
}

WindowTypeShelfItem::~WindowTypeShelfItem() = default;

// ExampleSearchResult is an app list search result. It provides what icon to
// show, what should title and details text look like. It also carries the
// matching window launch type so that AppListViewDelegate knows how to open
// it.
class ExampleSearchResult : public app_list::SearchResult {
 public:
  ExampleSearchResult(WindowTypeShelfItem::Type type,
                      const base::string16& query)
      : type_(type) {
    SetIcon(WindowTypeShelfItem::GetIcon(type_));

    base::string16 title =
        base::UTF8ToUTF16(WindowTypeShelfItem::GetTitle(type_));
    set_title(title);

    Tags title_tags;
    const size_t match_len = query.length();

    // Highlight matching parts in title with bold.
    // Note the following is not a proper way to handle i18n string.
    title = base::i18n::ToLower(title);
    size_t match_start = title.find(query);
    while (match_start != base::string16::npos) {
      title_tags.push_back(
          Tag(Tag::MATCH, match_start, match_start + match_len));
      match_start = title.find(query, match_start + match_len);
    }
    set_title_tags(title_tags);

    base::string16 details =
        base::UTF8ToUTF16(WindowTypeShelfItem::GetDetails(type_));
    set_details(details);
    Tags details_tags;
    details_tags.push_back(Tag(Tag::DIM, 0, details.length()));
    set_details_tags(details_tags);
  }

  WindowTypeShelfItem::Type type() const { return type_; }

 private:
  WindowTypeShelfItem::Type type_;

  DISALLOW_COPY_AND_ASSIGN(ExampleSearchResult);
};

class ExampleAppListViewDelegate : public app_list::AppListViewDelegate {
 public:
  ExampleAppListViewDelegate()
      : model_(std::make_unique<app_list::AppListModel>()),
        search_model_(std::make_unique<app_list::SearchModel>()) {
    PopulateApps();
    DecorateSearchBox(search_model_->search_box());
  }

 private:
  void PopulateApps() {
    for (int i = 0; i < static_cast<int>(WindowTypeShelfItem::LAST_TYPE); ++i) {
      WindowTypeShelfItem::Type type =
          static_cast<WindowTypeShelfItem::Type>(i);
      std::string id = base::IntToString(i);
      std::unique_ptr<WindowTypeShelfItem> shelf_item(
          new WindowTypeShelfItem(id, type));
      model_->AddItem(std::move(shelf_item));
    }
  }

  void DecorateSearchBox(app_list::SearchBoxModel* search_box_model) {
    search_box_model->SetHintText(base::ASCIIToUTF16("Type to search..."));
  }

  // Overridden from app_list::AppListViewDelegate:
  app_list::AppListModel* GetModel() override { return model_.get(); }

  app_list::SearchModel* GetSearchModel() override {
    return search_model_.get();
  }

  void OpenSearchResult(const std::string& result_id,
                        int event_flags) override {
    const ExampleSearchResult* example_result =
        static_cast<const ExampleSearchResult*>(
            search_model_->FindSearchResult(result_id));
    WindowTypeShelfItem::ActivateItem(example_result->type(), event_flags);
  }

  void InvokeSearchResultAction(const std::string& result_id,
                                int action_index,
                                int event_flags) override {
    NOTIMPLEMENTED();
  }

  void StartAssistant() override { NOTIMPLEMENTED(); }

  void StartSearch(const base::string16& raw_query) override {
    base::string16 query;
    base::TrimWhitespace(raw_query, base::TRIM_ALL, &query);
    query = base::i18n::ToLower(query);

    search_model_->results()->DeleteAll();
    if (query.empty())
      return;

    for (int i = 0; i < static_cast<int>(WindowTypeShelfItem::LAST_TYPE); ++i) {
      WindowTypeShelfItem::Type type =
          static_cast<WindowTypeShelfItem::Type>(i);

      base::string16 title =
          base::UTF8ToUTF16(WindowTypeShelfItem::GetTitle(type));
      if (base::i18n::StringSearchIgnoringCaseAndAccents(query, title, NULL,
                                                         NULL)) {
        search_model_->results()->Add(
            std::make_unique<ExampleSearchResult>(type, query));
      }
    }
  }

  void ViewShown(int64_t display_id) override {
    // Nothing needs to be done.
  }

  void DismissAppList() override {
    Shell::Get()->app_list_controller()->DismissAppList();
  }

  void ViewClosing() override {
    // Nothing needs to be done.
  }

  void ViewClosed() override {
    // Nothing needs to be done.
  }

  void GetWallpaperProminentColors(
      GetWallpaperProminentColorsCallback callback) override {
    NOTIMPLEMENTED();
  }

  void ActivateItem(const std::string& id, int event_flags) override {
    WindowTypeShelfItem* item =
        static_cast<WindowTypeShelfItem*>(model_->FindItem(id));
    if (!item)
      return;
    item->Activate(event_flags);
  }

  void GetContextMenuModel(const std::string& id,
                           GetContextMenuModelCallback callback) override {
    NOTIMPLEMENTED();
  }

  void GetSearchResultContextMenuModel(
      const std::string& result_id,
      GetContextMenuModelCallback callback) override {
    NOTIMPLEMENTED();
  }

  void ContextMenuItemSelected(const std::string& id,
                               int command_id,
                               int event_flags) override {
    NOTIMPLEMENTED();
  }

  void ShowWallpaperContextMenu(const gfx::Point& onscreen_location,
                                ui::MenuSourceType source_type) override {
    NOTIMPLEMENTED();
  }

  void SearchResultContextMenuItemSelected(const std::string& result_id,
                                           int command_id,
                                           int event_flags) override {
    NOTIMPLEMENTED();
  }

  bool ProcessHomeLauncherGesture(ui::GestureEvent* event,
                                  const gfx::Point& screen_location) override {
    NOTIMPLEMENTED();
    return false;
  }

  bool CanProcessEventsOnApplistViews() override {
    NOTIMPLEMENTED();
    return true;
  }

  ws::WindowService* GetWindowService() override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  std::unique_ptr<app_list::AppListModel> model_;
  std::unique_ptr<app_list::SearchModel> search_model_;

  DISALLOW_COPY_AND_ASSIGN(ExampleAppListViewDelegate);
};

}  // namespace

app_list::AppListViewDelegate* CreateAppListViewDelegate() {
  return new ExampleAppListViewDelegate;
}

}  // namespace shell
}  // namespace ash
