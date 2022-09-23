// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/price_tracking/shopping_list_ui_tab_helper.h"

#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

namespace commerce {

ShoppingListUiTabHelper::ShoppingListUiTabHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<ShoppingListUiTabHelper>(*contents) {}

ShoppingListUiTabHelper::~ShoppingListUiTabHelper() = default;

// static
void ShoppingListUiTabHelper::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kShouldShowPriceTrackFUEBubble, true);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ShoppingListUiTabHelper);

}  // namespace commerce
