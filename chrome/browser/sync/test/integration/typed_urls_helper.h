// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_TYPED_URLS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_TYPED_URLS_HELPER_H_

#include <vector>

#include "components/history/core/browser/history_types.h"
#include "ui/base/page_transition_types.h"

namespace base {
class Time;
}

// TODO(crbug.com/1365291): Rename this to history_helper.
namespace typed_urls_helper {

// Gets the URLRow for a specific URL from a specific sync profile. Returns
// false if the URL was not found in the history DB.
bool GetUrlFromClient(int index, const GURL& url, history::URLRow* row);

// Similar, but queries by URL ID rather than URL.
bool GetUrlFromClient(int index, history::URLID url_id, history::URLRow* row);

// Gets the visits for a URL from a specific sync profile.
history::VisitVector GetVisitsFromClient(int index, history::URLID id);

// Gets the visits for a URL from a specific sync profile. Like above, but
// takes a GURL instead of URLID. Returns empty vector if |url| is not returned
// by GetUrlFromClient().
history::VisitVector GetVisitsForURLFromClient(int index, const GURL& url);

// As above, but return `AnnotatedVisit` instead of just `VisitRow`.
std::vector<history::AnnotatedVisit> GetAnnotatedVisitsFromClient(
    int index,
    history::URLID id);
std::vector<history::AnnotatedVisit> GetAnnotatedVisitsForURLFromClient(
    int index,
    const GURL& url);

history::VisitVector GetRedirectChainFromClient(int index,
                                                history::VisitRow final_visit);

// Adds a URL to the history DB for a specific sync profile (just registers a
// new visit if the URL already exists) using a TYPED PageTransition.
void AddUrlToHistory(int index, const GURL& url);

// Adds a URL to the history DB for a specific sync profile (just registers a
// new visit if the URL already exists), using the passed PageTransition.
void AddUrlToHistoryWithTransition(int index,
                                   const GURL& url,
                                   ui::PageTransition transition,
                                   history::VisitSource source);

// Adds a URL to the history DB for a specific sync profile (just registers a
// new visit if the URL already exists), using the passed PageTransition and
// timestamp.
void AddUrlToHistoryWithTimestamp(int index,
                                  const GURL& url,
                                  ui::PageTransition transition,
                                  history::VisitSource source,
                                  const base::Time& timestamp);

}  // namespace typed_urls_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_TYPED_URLS_HELPER_H_
