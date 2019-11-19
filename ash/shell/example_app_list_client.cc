// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/example_app_list_client.h"

#include <algorithm>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/public/mojom/constants.mojom.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell/example_factory.h"
#include "ash/shell/toplevel_window.h"
#include "base/bind_helpers.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/string_search.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/examples_window.h"

namespace ash {
namespace shell {

// WindowTypeShelfItem is an app item of app list. It carries a window
// launch type and launches corresponding example window when activated.
class WindowTypeShelfItem : public AppListItem {
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
    icon.eraseColor(kColors[static_cast<int>(type) % base::size(kColors)]);
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

  Type type() const { return type_; }

 private:
  Type type_;

  DISALLOW_COPY_AND_ASSIGN(WindowTypeShelfItem);
};

WindowTypeShelfItem::WindowTypeShelfItem(const std::string& id, Type type)
    : AppListItem(id), type_(type) {
  std::string title(GetTitle(type));
  SetIcon(ash::AppListConfigType::kShared, GetIcon(type));
  SetName(title);
}

WindowTypeShelfItem::~WindowTypeShelfItem() = default;

// ExampleSearchResult is an app list search result. It provides what icon to
// show, what should title and details text look like. It also carries the
// matching window launch type so that AppListViewDelegate knows how to open
// it.
class ExampleSearchResult : public SearchResult {
 public:
  ExampleSearchResult(WindowTypeShelfItem::Type type,
                      const base::string16& query)
      : type_(type) {
    SetIcon(WindowTypeShelfItem::GetIcon(type_));

    base::string16 title =
        base::UTF8ToUTF16(WindowTypeShelfItem::GetTitle(type_));
    set_title(title);

    if (query.empty()) {
      set_display_type(ash::SearchResultDisplayType::kRecommendation);
      SetChipIcon(WindowTypeShelfItem::GetIcon(type_));
    } else {
      Tags title_tags;

      // Highlight matching parts in title with bold.
      // Note the following is not a proper way to handle i18n string.
      title = base::i18n::ToLower(title);
      const size_t match_len = query.length();
      size_t match_start = title.find(query);
      while (match_start != base::string16::npos) {
        title_tags.push_back(
            Tag(Tag::MATCH, match_start, match_start + match_len));
        match_start = title.find(query, match_start + match_len);
      }
      set_title_tags(title_tags);
    }

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

ExampleAppListClient::ExampleAppListClient(AppListControllerImpl* controller)
    : controller_(controller) {
  controller_->SetClient(this);

  PopulateApps();
  DecorateSearchBox();
}

ExampleAppListClient::~ExampleAppListClient() {
  controller_->SetClient(nullptr);
}

void ExampleAppListClient::PopulateApps() {
  for (int i = 0; i < static_cast<int>(WindowTypeShelfItem::LAST_TYPE); ++i) {
    WindowTypeShelfItem::Type type = static_cast<WindowTypeShelfItem::Type>(i);
    const std::string id = base::NumberToString(i);
    auto app = std::make_unique<WindowTypeShelfItem>(id, type);
    controller_->AddItem(app->CloneMetadata());
    apps_.emplace_back(std::move(app));
  }
}

void ExampleAppListClient::DecorateSearchBox() {
  controller_->SetSearchHintText(base::ASCIIToUTF16("Type to search..."));
}

void ExampleAppListClient::StartSearch(const base::string16& trimmed_query) {
  base::string16 query;
  query = base::i18n::ToLower(trimmed_query);

  search_results_.clear();
  std::vector<std::unique_ptr<ash::SearchResultMetadata>> result_data;
  for (int i = 0; i < static_cast<int>(WindowTypeShelfItem::LAST_TYPE); ++i) {
    WindowTypeShelfItem::Type type = static_cast<WindowTypeShelfItem::Type>(i);

    const base::string16 title =
        base::UTF8ToUTF16(WindowTypeShelfItem::GetTitle(type));
    if (query.empty() || base::i18n::StringSearchIgnoringCaseAndAccents(
                             query, title, nullptr, nullptr)) {
      search_results_.emplace_back(
          std::make_unique<ExampleSearchResult>(type, query));
      result_data.emplace_back(search_results_.back()->CloneMetadata());
    }
  }
  controller_->PublishSearchResults(std::move(result_data));
}

void ExampleAppListClient::OpenSearchResult(
    const std::string& result_id,
    int event_flags,
    ash::AppListLaunchedFrom launched_from,
    ash::AppListLaunchType launch_type,
    int suggestion_index,
    bool launch_as_default) {
  auto it = std::find_if(
      search_results_.begin(), search_results_.end(),
      [&result_id](const std::unique_ptr<ExampleSearchResult>& result) {
        return result->id() == result_id;
      });
  if (it == search_results_.end())
    return;

  WindowTypeShelfItem::ActivateItem((*it)->type(), event_flags);
}

void ExampleAppListClient::ActivateItem(int profile_id,
                                        const std::string& id,
                                        int event_flags) {
  auto it =
      std::find_if(apps_.begin(), apps_.end(),
                   [&id](const std::unique_ptr<WindowTypeShelfItem>& app) {
                     return app->id() == id;
                   });
  if (it == apps_.end())
    return;

  WindowTypeShelfItem::ActivateItem((*it)->type(), event_flags);
}

}  // namespace shell
}  // namespace ash
