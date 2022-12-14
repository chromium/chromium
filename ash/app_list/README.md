# App List

"Launcher" is the user-visible name for this feature.

[TOC]

## Feature summary

*   Apps exist on a list of pages. Each page can be partially full. Empty space
    appears at the end of each page. The app list does not support Android-style
    "holes" in the middle of a page.
*   Each page is a fixed-size grid (commonly 5x4).
*   Apps can be reordered by dragging. If an app is dragged below the last page,
    a new page is created. If a page's last app is removed, the page disappears.
*   Folders can be created by dragging an app on top of another app. Folders are
    one level deep (no folders inside folders). A folder that contains a large
    number of items shows pages which scroll horizontally.
*   Folders generally contain 2 or more items. When the second-to-last item from
    a folder is removed, the folder is deleted and the remaining items appears
    on the main app grid. Some special folders are allowed to contain 1 item
    (e.g. "Linux Apps").
*   The app list is navigable with arrow keys. Apps can be reordered with
    Ctrl+arrow key. Folders can be created with Ctrl+Shift+arrow key. In
    addition to accessibility, keyboard shortcuts are helpful for quickly
    creating folders with large numbers of items.
*   The list of apps and their order is synced between devices. However, some
    built-in OEM apps do not appear on all devices (e.g. an HP-only app might
    not show up on an Acer Chromebook).
*   Default-installed apps may be deleted.

## Code structure

`//ash/app_list` contains the view structure and data model. Before 2018 this
code lived in `//ui/app_list` because the app list used to be supported on
non-Chrome OS platforms.

`//chrome/browser/ash/app_list` contains app list code that has Profile
dependencies. This includes sync support and communication with the App Service
(which provides the list of installed apps).

## Data model

### Apps

The list of installed apps is provided by the [App Service][1]. It includes a
variety of app types:

*   Built-in apps
*   Progressive web apps (PWAs)
*   Android apps (via ARC++)
*   Linux apps (via Crostini)
*   Deprecated platform apps ("Chrome Apps", turning down in 2022)
*   Extension-based apps (a.k.a. bookmark apps)
*   "Remote" apps (used in enterprise environments, see
    [bug](https://crbug.com/1101208) for details)

Some of a user's apps might not be supported on their current device. For
example, a user might have a device that does not support Crostini. Likewise,
they might have a device on a new OS version (e.g. dev channel) that includes a
new built-in app but also have devices on older OS versions that do not support
that app.

Unsupported apps are not shown in the app list.

[1]: components/services/app_service/README.md

### Sync data

See the [AppListSpecifics protocol
buffer](/components/sync/protocol/app_list_specifics.proto)

*   A sync item can be an app, a "remove default app" request, a folder, or a
    page break.
*   Items have an extension-style string id (e.g. Chrome Web Store is
    "ahfgeienlihckogmohjhadlkjgocpleb")
*   The sync data does not contain an ordered list of items. Instead, each
    individual item has a string "ordinal" that the client uses to sort the
    views.
*   Pagination is handled via page break items.
*   Items that appear in folders store the id of the containing folder.
*   OEM apps always appear in the OEM folder, even though they might have a
    different parent (or no parent) in the sync data. One reason is because the
    same app might be considered an OEM app on device A, but not an OEM app on
    device B.
*   Items have a "pin ordinal", used to pin and sort pinned apps on the shelf.

Note that the sync data does not contain which page an app is on, nor the app's
position within a page.

#### Ordinals

For the app list, an ordinal is a string type that allows ordering and insertion
without rewriting existing items. For example, with ordinals "aa" and "bb" you
can create an ordinal "am" that sorts to the middle, without changing "aa" or
"bb".

### App list model

[ash::AppListModel][1] is the core data model. There is a single copy of this
model, owned by ash.

AppListModel owns an [AppListItemList][2] for the top-level grid of apps.
AppListItemList contains items in the order they appear in the app list, across
across all pages.

Each [AppListItem][3] contains [AppListItemMetadata][4]. The data is similar to
the data provided by sync, but is more focused on display. As of March 2021 the
data includes:

*   id (extension-style string, matching the sync item id)
*   name
*   app_status (e.g. ready/blocked/paused)
*   folder_id (a UUID, e.g. 5e47865b-c00b-4fd9-ac90-e174e1d28aad)
*   position (a string ordinal)
*   icon
*   type information (folder, persistent folder, page break)

The ash data model is not directly exposed to code in `//chrome`. Chrome has its
own data about each item, with [ChromeAppListModelUpdater][5] owning a map of
[ChromeAppListItem][6]. These items use the same metadata as AppListItem. This
separation is left over from the mustash project, where code in `//ash` and
`//chrome` used to run in separate processes, and hence could not directly share
a model. See [go/move-applist][7].

[1]: /ash/app_list/model/app_list_model.h
[2]: /ash/app_list/model/app_list_item_list.h
[3]: /ash/app_list/model/app_list_item.h
[4]: /ash/public/cpp/app_list/app_list_types.h
[5]: /chrome/browser/ash/app_list/chrome_app_list_model_updater.h
[6]: /chrome/browser/ash/app_list/chrome_app_list_item.h
[7]: http://go/move-applist

#### Folders

[AppListFolderItem][1] is a subclass of [AppListItem][2]. Each folder has its
own [AppListItemList][3]. Items inside of folders do not appear in the top-level
item list.

Folders do not contain page breaks. Each page must be filled before the next
page is created.

While items inside a folder can be reordered, the order data is not persisted to
sync.

[1]: /ash/app_list/model/app_list_folder_item.h
[2]: /ash/app_list/model/app_list_item.h
[3]: /ash/app_list/model/app_list_item_list.h

## Views

An [AppListItemView][1] represents each app. It is a button and has an image
icon and a name label.

[AppsGridView][2] displays a grid of AppListItemViews. An AppsGridView is used
to show the main app grid. A separate AppsGridView is used to show the contents
of a folder.

AppsGridView has an AppListItemView for each app in the main list, even those
that are not on the current page (and hence are not visible). AppsGridView also
contains a [PaginationModel][3], which has a list of views for each visual page.

When a folder is open, its [AppListFolderView][4] is stacked on top of the main
apps grid view. Only one folder can be open at a time. The folder view contains
its own AppsGridView.

Therefore the view hierarchy is approximately this:

*   AppsContainerView
    *   (Suggestion related views)
    *   AppsGridView
        *   AppListItemView
        *   AppListItemView
        *   ...
    *   (Page switcher related views)
    *   AppListFolderView
        *   AppsGridView
            *   AppListItemView
            *   AppListItemView
            *   ...

You can run chrome with --ash-debug-shortcuts, open the launcher, and press
Ctrl-Alt-Shift-V to see the full view hierarchy.

[1]: /ash/app_list/views/app_list_item_view.h
[2]: /ash/app_list/views/apps_grid_view.h
[3]: /ash/public/cpp/pagination/pagination_model.h
[4]: /ash/app_list/views/app_list_folder_view.h

## Testing

App list tests live in ash_unittests. Run the unit tests with:

    testing/xvfb.py out/Default/ash_unittests

Tests for high level user actions (reordering icons, creating folders, etc.) are
generally part of [apps_grid_view_unittest.cc][1] or
[app_list_presenter_delegate_unittest.cc][2].

[1]: /ash/app_list/views/apps_grid_view_unittest.cc
[2]: /ash/app_list/app_list_presenter_delegate_unittest.cc

## Historical notes

The old demo binary in //ash/app_list/demo was removed in 2021.

The shelf was originally called the launcher (circa 2012).
