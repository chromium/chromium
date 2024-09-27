// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_REMOVER_CONSTANTS_H_
#define CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_REMOVER_CONSTANTS_H_

#include <stdint.h>

#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "extensions/buildflags/buildflags.h"

namespace chrome_browsing_data_remover {
// This is an extension of content::BrowsingDataRemover::RemoveDataMask which
// includes all datatypes therefrom and adds additional Chrome-specific ones.
using DataType = uint64_t;
  // Embedder can start adding datatypes after the last platform datatype.
constexpr DataType DATA_TYPE_EMBEDDER_BEGIN =
    content::BrowsingDataRemover::DATA_TYPE_CONTENT_END << 1;

// Chrome-specific datatypes.
constexpr DataType DATA_TYPE_HISTORY = DATA_TYPE_EMBEDDER_BEGIN;
constexpr DataType DATA_TYPE_FORM_DATA = DATA_TYPE_EMBEDDER_BEGIN << 1;
constexpr DataType DATA_TYPE_PASSWORDS = DATA_TYPE_EMBEDDER_BEGIN << 2;
#if BUILDFLAG(IS_ANDROID)
constexpr DataType DATA_TYPE_WEB_APP_DATA = DATA_TYPE_EMBEDDER_BEGIN << 3;
#endif
constexpr DataType DATA_TYPE_SITE_USAGE_DATA = DATA_TYPE_EMBEDDER_BEGIN << 4;
constexpr DataType DATA_TYPE_DURABLE_PERMISSION = DATA_TYPE_EMBEDDER_BEGIN << 5;
constexpr DataType DATA_TYPE_EXTERNAL_PROTOCOL_DATA = DATA_TYPE_EMBEDDER_BEGIN
                                                      << 6;
constexpr DataType DATA_TYPE_HOSTED_APP_DATA_TEST_ONLY =
    DATA_TYPE_EMBEDDER_BEGIN << 7;
constexpr DataType DATA_TYPE_CONTENT_SETTINGS = DATA_TYPE_EMBEDDER_BEGIN << 8;
constexpr DataType DATA_TYPE_BOOKMARKS = DATA_TYPE_EMBEDDER_BEGIN << 9;
constexpr DataType DATA_TYPE_ISOLATED_ORIGINS = DATA_TYPE_EMBEDDER_BEGIN << 10;
constexpr DataType DATA_TYPE_ACCOUNT_PASSWORDS = DATA_TYPE_EMBEDDER_BEGIN << 11;
constexpr DataType DATA_TYPE_LOCAL_CUSTOM_DICTIONARY = DATA_TYPE_EMBEDDER_BEGIN
                                                       << 12;
constexpr DataType DATA_TYPE_ISOLATED_WEB_APP_COOKIES = DATA_TYPE_EMBEDDER_BEGIN
                                                        << 13;
constexpr DataType DATA_TYPE_READING_LIST = DATA_TYPE_EMBEDDER_BEGIN << 14;
constexpr DataType DATA_TYPE_TABS = DATA_TYPE_EMBEDDER_BEGIN << 15;
constexpr DataType DATA_TYPE_SEARCH_ENGINE_CHOICE = DATA_TYPE_EMBEDDER_BEGIN
                                                    << 16;

// Group datatypes.

// "Site data" includes storage backend accessible to websites and some
// additional metadata kept by the browser (e.g. site usage data).
constexpr DataType DATA_TYPE_SITE_DATA =
    content::BrowsingDataRemover::DATA_TYPE_COOKIES |
    content::BrowsingDataRemover::DATA_TYPE_DOM_STORAGE |
    content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES |
#if BUILDFLAG(IS_ANDROID)
    DATA_TYPE_WEB_APP_DATA |
#endif
    DATA_TYPE_SITE_USAGE_DATA | DATA_TYPE_DURABLE_PERMISSION |
    DATA_TYPE_EXTERNAL_PROTOCOL_DATA | DATA_TYPE_ISOLATED_ORIGINS |
    DATA_TYPE_ISOLATED_WEB_APP_COOKIES |
    content::BrowsingDataRemover::DATA_TYPE_PRIVACY_SANDBOX;

// Datatypes protected by Important Sites.
constexpr DataType IMPORTANT_SITES_DATA_TYPES =
    DATA_TYPE_SITE_DATA | content::BrowsingDataRemover::DATA_TYPE_CACHE;

// Datatypes that can be deleted partially per URL / origin / domain,
// whichever makes sense.
constexpr DataType FILTERABLE_DATA_TYPES =
    DATA_TYPE_SITE_DATA | content::BrowsingDataRemover::DATA_TYPE_CACHE |
    content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS |
    content::BrowsingDataRemover::DATA_TYPE_RELATED_WEBSITE_SETS_PERMISSIONS;

// Datatypes with account-scoped data that needs to be removed
// before Google cookies are deleted.
constexpr DataType DEFERRED_COOKIE_DELETION_DATA_TYPES =
    DATA_TYPE_ACCOUNT_PASSWORDS;

// Includes all the available remove options. Meant to be used by clients
// that wish to wipe as much data as possible from a Profile, to make it
// look like a new Profile. Does not delete account-scoped data like
// passwords but will remove access to account-scoped data by signing the
// user out.
constexpr DataType ALL_DATA_TYPES =
    DATA_TYPE_SITE_DATA |  //
    content::BrowsingDataRemover::DATA_TYPE_CACHE |
    content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS |
    DATA_TYPE_FORM_DATA |                //
    DATA_TYPE_HISTORY |                  //
    DATA_TYPE_PASSWORDS |                //
    DATA_TYPE_CONTENT_SETTINGS |         //
    DATA_TYPE_BOOKMARKS |                //
    DATA_TYPE_LOCAL_CUSTOM_DICTIONARY |  //
    DATA_TYPE_READING_LIST |             //
    DATA_TYPE_SEARCH_ENGINE_CHOICE;

// Includes all available remove options. Meant to be used when the Profile
// is scheduled to be deleted, and all possible data should be wiped from
// disk as soon as possible.
constexpr DataType WIPE_PROFILE =
    ALL_DATA_TYPES | content::BrowsingDataRemover::DATA_TYPE_NO_CHECKS;

// This is an extension of content::BrowsingDataRemover::OriginType which
// includes all origin types therefrom and adds additional Chrome-specific
// ones.
using OriginType = uint64_t;
// Embedder can start adding origin types after the last
// platform origin type.
constexpr OriginType ORIGIN_TYPE_EMBEDDER_BEGIN =
    content::BrowsingDataRemover::ORIGIN_TYPE_CONTENT_END << 1;

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Packaged apps and extensions (chrome-extension://*).
constexpr OriginType ORIGIN_TYPE_EXTENSION = ORIGIN_TYPE_EMBEDDER_BEGIN;
#endif

  // All origin types.
constexpr OriginType ALL_ORIGIN_TYPES =
    content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
#if BUILDFLAG(ENABLE_EXTENSIONS)
    ORIGIN_TYPE_EXTENSION |
#endif
    content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;

// Important sites protect a small set of sites from the deletion of certain
// datatypes. Therefore, those datatypes must be filterable by
// url/origin/domain.
static_assert((IMPORTANT_SITES_DATA_TYPES & ~FILTERABLE_DATA_TYPES) == 0,
              "All important sites datatypes must be filterable.");

static_assert((DEFERRED_COOKIE_DELETION_DATA_TYPES & FILTERABLE_DATA_TYPES) ==
                  0,
              "Deferred deletion is currently not implemented for filterable "
              "data types");

static_assert((DEFERRED_COOKIE_DELETION_DATA_TYPES & WIPE_PROFILE) == 0,
              "Account data should not be included in deletions that remove "
              "all local data");
}  // namespace chrome_browsing_data_remover

#endif  // CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_REMOVER_CONSTANTS_H_
