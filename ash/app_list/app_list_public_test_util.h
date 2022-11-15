// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_PUBLIC_TEST_UTIL_H_
#define ASH_APP_LIST_APP_LIST_PUBLIC_TEST_UTIL_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_types.h"

namespace ash {

class AppListBubbleView;
class AppListView;
class SearchBoxView;

// An app list should be either a bubble app list or a fullscreen app list.
// Returns true if a bubble app list should be used under the current mode.
ASH_EXPORT bool ShouldUseBubbleAppList();

// Fetches the app list bubble view. Used in clamshell mode.
ASH_EXPORT AppListBubbleView* GetAppListBubbleView();

// Fetches the app list view. Used in tablet mode only app list view.
ASH_EXPORT AppListView* GetAppListView();

// Fetches the launcher search box.
ASH_EXPORT SearchBoxView* GetSearchBoxView();

// Fetches the launcher search box's ghost text autocomplete and category
// contents.
ASH_EXPORT std::string GetSearchBoxGhostTextForTest();

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_PUBLIC_TEST_UTIL_H_
