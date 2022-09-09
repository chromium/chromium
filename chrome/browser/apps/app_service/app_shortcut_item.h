// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SHORTCUT_ITEM_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SHORTCUT_ITEM_H_

#include <string>
#include <vector>

#include "ash/components/arc/mojom/app.mojom.h"
#include "ui/gfx/image/image_skia.h"

namespace apps {

// Describes app shortcut that is published by Android's ShortcutManager.
struct AppShortcutItem {
  AppShortcutItem();
  AppShortcutItem(const AppShortcutItem& item);
  ~AppShortcutItem();

  // The ID of this shortcut. Unique within each publisher app and stable across
  // devices.
  std::string shortcut_id;

  // The short description of this shortcut.
  std::string short_label;

  // The icon for this shortcut.
  gfx::ImageSkia icon;

  // The category type of this shortcut.
  arc::mojom::AppShortcutItemType type =
      arc::mojom::AppShortcutItemType::kStatic;

  // "Rank" of a shortcut, which is a non-negative, sequential value.
  int rank = 0;
};

using AppShortcutItems = std::vector<AppShortcutItem>;

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_SHORTCUT_ITEM_H_
