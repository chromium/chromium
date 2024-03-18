// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_icon_container.h"

#include <algorithm>
#include <cstdint>

#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/desks/templates/saved_desk_constants.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "components/app_restore/app_restore_utils.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Given a map of unique icon identifiers to icon info, returns a vector of the
// same key, value pair ordered by icons' activation index.
std::vector<SavedDeskIconContainer::IconIdentifierAndIconInfo>
SortIconIdentifierToIconInfo(
    std::map<SavedDeskIconIdentifier, SavedDeskIconContainer::IconInfo>&&
        icon_identifier_to_icon_info) {
  // Create a vector using `sorted_icon_identifier_to_icon_info` that contains
  // pairs of identifiers and counts. This will initially be unsorted.
  std::vector<SavedDeskIconContainer::IconIdentifierAndIconInfo>
      sorted_icon_identifier_to_icon_info;

  for (auto& entry : icon_identifier_to_icon_info) {
    sorted_icon_identifier_to_icon_info.emplace_back(std::move(entry.first),
                                                     std::move(entry.second));
  }

  // Sort by activation index.
  base::ranges::sort(
      sorted_icon_identifier_to_icon_info, {},
      [](const SavedDeskIconContainer::IconIdentifierAndIconInfo& a) {
        return a.second.activation_index;
      });

  return sorted_icon_identifier_to_icon_info;
}

// Inserts an `IconInfo` struct into `out_icon_identifier_to_icon_info` if no
// entry exists for `identifier`. If an entry exists for `identifier`, updates
// its values.
void InsertIconIdentifierToIconInfo(
    const std::string& app_id,
    const std::u16string& app_title,
    const SavedDeskIconIdentifier& identifier,
    int activation_index,
    std::map<SavedDeskIconIdentifier, SavedDeskIconContainer::IconInfo>&
        out_icon_identifier_to_icon_info) {
  // A single app/site can have multiple windows so count their occurrences and
  // use the smallest activation index for sorting purposes.
  if (!base::Contains(out_icon_identifier_to_icon_info, identifier)) {
    out_icon_identifier_to_icon_info[identifier] = {
        app_id, base::UTF16ToUTF8(app_title), activation_index,
        /*count=*/1};
  } else {
    auto& entry = out_icon_identifier_to_icon_info[identifier];
    ++entry.count;
    entry.activation_index = std::min(entry.activation_index, activation_index);
  }
}

// Iterates through `launch_list`, inserting `IconInfo` structs into
// `out_icon_identifier_to_icon_info` for each tab and app.
void InsertIconIdentifierToIconInfoFromLaunchList(
    const std::string& app_id,
    const app_restore::RestoreData::LaunchList& launch_list,
    std::map<SavedDeskIconIdentifier, SavedDeskIconContainer::IconInfo>&
        out_icon_identifier_to_icon_info) {
  // We want to group active tabs and apps ahead of inactive tabs so offsets
  // inactive tabs activation index by `kInactiveTabOffset`. In almost every use
  // case, there should be no more than `kInactiveTabOffset` number of tabs +
  // apps on a desk.
  constexpr int kInactiveTabOffset = 10000;

  for (auto& restore_data : launch_list) {
    // If `restore_data` is a SWA then it will have a valid url for its active
    // tab. However, in this case we want to display the SWA's icon via its app
    // id so to determine whether `restore_data` is an SWA we need to check
    // whether it's a browser.
    const app_restore::BrowserExtraInfo& browser_extra_info =
        restore_data.second->browser_extra_info;
    const bool is_browser =
        IsBrowserAppId(app_id) &&
        !browser_extra_info.app_type_browser.value_or(false);
    const int activation_index =
        restore_data.second->window_info.activation_index.value_or(0);
    const int active_tab_index =
        browser_extra_info.active_tab_index.value_or(-1);
    const std::u16string app_title =
        restore_data.second->window_info.app_title.value_or(u"");
    if (!restore_data.second->browser_extra_info.urls.empty() && is_browser) {
      const auto& urls = browser_extra_info.urls;
      // Make all urls that have the same domain identical.
      std::map<GURL, size_t> domain_to_url_index;
      for (int i = 0; i < static_cast<int>(urls.size()); ++i) {
        // The first URL that we see for a domain is the one we will use for all
        // entries with the same domain. If this is the first URL for a domain,
        // then `it` will point to the newly inserted entry, otherwise it will
        // point to an already existing entry.
        auto [it, found] =
            domain_to_url_index.emplace(urls[i].GetWithEmptyPath(), i);
        const GURL& url = urls[it->second];

        InsertIconIdentifierToIconInfo(
            app_id, app_title,
            {.url_or_id = url.spec(),
             .lacros_profile_id =
                 browser_extra_info.lacros_profile_id.value_or(0)},
            active_tab_index == i ? activation_index
                                  : kInactiveTabOffset + activation_index,
            out_icon_identifier_to_icon_info);
      }
    } else {
      // PWAs will have the same app id as chrome. For these apps, retrieve
      // their app id from their app name if possible.
      std::string new_app_id = app_id;
      const std::optional<std::string>& app_name = browser_extra_info.app_name;
      if (IsBrowserAppId(app_id) && app_name.has_value())
        new_app_id = app_restore::GetAppIdFromAppName(app_name.value());

      InsertIconIdentifierToIconInfo(
          app_id, app_title, {.url_or_id = new_app_id}, activation_index,
          out_icon_identifier_to_icon_info);
    }
  }
}

}  // namespace

SavedDeskIconContainer::SavedDeskIconContainer() {
  views::Builder<SavedDeskIconContainer>(this)
      .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
      .SetBetweenChildSpacing(kSaveDeskSpacingDp)
      .BuildChildren();
}

SavedDeskIconContainer::~SavedDeskIconContainer() = default;

void SavedDeskIconContainer::Layout(PassKey) {
  LayoutSuperclass<views::BoxLayoutView>(this);

  // At this point we can not guarantee whether the child icon view has done
  // its icon loading yet, but `SortIconsAndUpdateOverflowIcon()` will be
  // invoked as an `OnceCallback` to ensure the newly loaded default icon gets
  // moved to the end, and the overflow icon updates correctly.
  SortIconsAndUpdateOverflowIcon();
}

void SavedDeskIconContainer::PopulateIconContainerFromTemplate(
    const DeskTemplate* desk_template) {
  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  if (!restore_data)
    return;

  // Iterate through the template's WindowInfo, counting the occurrences of each
  // unique icon identifier and storing their lowest activation index.
  std::map<SavedDeskIconIdentifier, IconInfo> icon_identifier_to_icon_info;
  const auto& launch_list = restore_data->app_id_to_launch_list();
  for (auto& app_id_to_launch_list_entry : launch_list) {
    InsertIconIdentifierToIconInfoFromLaunchList(
        app_id_to_launch_list_entry.first, app_id_to_launch_list_entry.second,
        icon_identifier_to_icon_info);
  }

  CreateIconViewsFromIconIdentifiers(
      SortIconIdentifierToIconInfo(std::move(icon_identifier_to_icon_info)));
}

void SavedDeskIconContainer::PopulateIconContainerFromWindows(
    const std::vector<raw_ptr<aura::Window, VectorExperimental>>& windows) {
  DCHECK(!windows.empty());

  // Iterate through `windows`, counting the occurrences of each unique icon and
  // storing their lowest activation index.
  std::map<SavedDeskIconIdentifier, IconInfo> icon_identifier_to_icon_info;
  auto* delegate = Shell::Get()->saved_desk_delegate();
  for (size_t i = 0; i < windows.size(); ++i) {
    auto* window = windows[i].get();

    // If `window` is an incognito window, we want to display the incognito icon
    // instead of its favicons so denote it using
    // `DeskTemplate::kIncognitoWindowIdentifier`.
    const bool is_window_persistable = delegate->IsWindowPersistable(window);
    const std::string app_id =
        is_window_persistable
            ? ShelfID::Deserialize(window->GetProperty(kShelfIDKey)).app_id
            : DeskTemplate::kIncognitoWindowIdentifier;
    if (!is_window_persistable && !incognito_window_color_provider_) {
      incognito_window_color_provider_ =
          views::Widget::GetWidgetForNativeWindow(window)->GetColorProvider();
    }

    // Since there were no modifications to `app_id`, app id and icon identifier
    // are both `app_id`.
    InsertIconIdentifierToIconInfo(
        /*app_id=*/app_id, /*app_title=*/window->GetTitle(),
        /*identifier=*/{.url_or_id = app_id}, i, icon_identifier_to_icon_info);
  }

  CreateIconViewsFromIconIdentifiers(
      SortIconIdentifierToIconInfo(std::move(icon_identifier_to_icon_info)));
}

std::vector<SavedDeskIconView*> SavedDeskIconContainer::GetIconViews() const {
  std::vector<SavedDeskIconView*> icon_views;
  icon_views.reserve(children().size());
  base::ranges::for_each(children(), [&icon_views](views::View* view) {
    icon_views.emplace_back(static_cast<SavedDeskIconView*>(view));
  });

  DCHECK(!icon_views.empty());
  DCHECK(overflow_icon_view_);
  DCHECK(overflow_icon_view_ == icon_views.back());

  return icon_views;
}

void SavedDeskIconContainer::OnViewLoaded(views::View* view_loaded) {
  auto it = base::ranges::find(children(), view_loaded);
  DCHECK(it != children().end());

  SortIconsAndUpdateOverflowIcon();
}

void SavedDeskIconContainer::SortIconsAndUpdateOverflowIcon() {
  if (overflow_icon_view_) {
    SortIcons();
    UpdateOverflowIcon();
  }
}

void SavedDeskIconContainer::SortIcons() {
  // Make a cope of child icon views and sort them using
  // `SavedDeskIconView::key`.
  std::vector<SavedDeskIconView*> icon_views = GetIconViews();
  base::ranges::sort(icon_views, {}, &SavedDeskIconView::GetSortingKey);

  // Update child views to their expected index.
  for (size_t i = 0; i < icon_views.size(); i++)
    ReorderChildView(icon_views[i], i);

  // Notify the a11y API so that the spoken feedback order matches the view
  // order.
  NotifyAccessibilityEvent(ax::mojom::Event::kTreeChanged, true);
}

void SavedDeskIconContainer::UpdateOverflowIcon() {
  std::vector<SavedDeskIconView*> icon_views = GetIconViews();

  int num_hidden_apps = uncreated_app_count_;
  int num_shown_icons = icon_views.size() - 1;
  SavedDeskIconView* overflow_icon_view =
      static_cast<SavedDeskIconView*>(overflow_icon_view_);
  const int available_width = bounds().width();
  int used_width = -kSaveDeskSpacingDp;
  base::ranges::for_each(
      icon_views, [&used_width](SavedDeskIconView* icon_view) {
        if (!icon_view->IsOverflowIcon()) {
          used_width +=
              icon_view->GetPreferredSize().width() + kSaveDeskSpacingDp;
        }
      });

  // Go through all non-overflow icons from back to front, and hide if:
  //   1) needed width is greater than given;
  //   2) number of shown icons is greater than `kMaxIcons`;
  // During the scan, the width needed for overflow icon is adjusted based on
  // count, and visibility of all icons will be reset.
  for (int i = static_cast<int>(icon_views.size()) - 2; i >= 0; i--) {
    int needed_overflow_icon_width = 0;
    if (overflow_icon_view->GetCount()) {
      needed_overflow_icon_width =
          overflow_icon_view_->GetPreferredSize().width() + kSaveDeskSpacingDp;
    }
    if (used_width + needed_overflow_icon_width > available_width ||
        num_shown_icons > kMaxIcons) {
      if (icon_views[i]->GetVisible())
        icon_views[i]->SetVisible(false);
      used_width -=
          icon_views[i]->GetPreferredSize().width() + kSaveDeskSpacingDp;
      num_hidden_apps += icon_views[i]->GetCount();
      num_shown_icons--;
      overflow_icon_view->UpdateCount(num_hidden_apps);
    } else {
      if (!icon_views[i]->GetVisible())
        icon_views[i]->SetVisible(true);
    }
  }

  if (num_hidden_apps == 0)
    overflow_icon_view->SetVisible(false);
  else
    overflow_icon_view->SetVisible(true);
}

void SavedDeskIconContainer::CreateIconViewsFromIconIdentifiers(
    const std::vector<IconIdentifierAndIconInfo>&
        icon_identifier_to_icon_info) {
  DCHECK(children().empty());

  if (icon_identifier_to_icon_info.empty())
    return;

  auto* delegate = Shell::Get()->saved_desk_delegate();
  uncreated_app_count_ = 0;
  for (size_t i = 0; i < icon_identifier_to_icon_info.size(); i++) {
    const auto& [icon_identifier, icon_info] = icon_identifier_to_icon_info[i];
    // Don't create new icons if the app is unavailable (uninstalled or
    // unsupported). Count the amount of skipped apps so we know what to display
    // on the overflow. In addition, dialog popups may show incognito window
    // icons. Saved desks will not have incognito window icon identifiers and
    // will not count them here. For `sorting_key`, use `i` because we would
    // like to preserve the original order.
    if (icon_identifier.url_or_id == DeskTemplate::kIncognitoWindowIdentifier ||
        IsBrowserAppId(icon_info.app_id) ||
        delegate->IsAppAvailable(icon_info.app_id)) {
      AddChildView(std::make_unique<SavedDeskRegularIconView>(
          /*incognito_window_color_provider=*/incognito_window_color_provider_,
          /*icon_identifier=*/icon_identifier,
          /*app_title=*/icon_info.app_title,
          /*count=*/icon_info.count,
          /*sorting_key=*/i,
          /*on_icon_loaded=*/
          base::BindOnce(&SavedDeskIconContainer::OnViewLoaded,
                         weak_ptr_factory_.GetWeakPtr())));
    } else {
      uncreated_app_count_ += icon_info.count;
    }
  }

  // Always add a `SavedDeskIconView` overflow counter in case the width
  // of the view changes. It will be hidden if not needed. For `show_plus`, If
  // no child views were added, the icon container contains only unavailable
  // apps so we should *not* show plus.
  overflow_icon_view_ =
      AddChildView(std::make_unique<SavedDeskOverflowIconView>(
          /*count=*/uncreated_app_count_,
          /*show_plus=*/!children().empty()));
}

BEGIN_METADATA(SavedDeskIconContainer)
END_METADATA

}  // namespace ash
