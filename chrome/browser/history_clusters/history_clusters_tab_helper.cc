// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_clusters/history_clusters_tab_helper.h"

#include "components/history/core/browser/history_types.h"

HistoryClustersTabHelper::HistoryClustersTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

HistoryClustersTabHelper::~HistoryClustersTabHelper() = default;

void HistoryClustersTabHelper::WillUpdateHistoryForNavigation(
    content::NavigationHandle* navigation_handle,
    const history::HistoryAddPageArgs& add_page_args) {}

void HistoryClustersTabHelper::DidUpdateHistoryForNavigation(
    content::NavigationHandle* navigation_handle,
    const history::HistoryAddPageArgs& add_page_args) {}

void HistoryClustersTabHelper::WebContentsDestroyed() {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HistoryClustersTabHelper)
