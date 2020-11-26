// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_REMOVER_CONSTANTS_H_
#define CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_REMOVER_CONSTANTS_H_

#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "content/public/browser/browsing_data_remover.h"
#include "extensions/buildflags/buildflags.h"

namespace chrome_browsing_data_remover {
// This is an extension of content::BrowsingDataRemover::RemoveDataMask which
// includes all datatypes therefrom and adds additional Chrome-specific ones.
enum DataType : uint64_t {
  // Embedder can start adding datatypes after the last platform datatype.
  DATA_TYPE_EMBEDDER_BEGIN = content::BrowsingDataRemover::DATA_TYPE_CONTENT_END
                             << 1,

  // Chrome-specific datatypes.
  DATA_TYPE_HISTORY = DATA_TYPE_EMBEDDER_BEGIN,
  DATA_TYPE_FORM_DATA = DATA_TYPE_EMBEDDER_BEGIN << 1,
  DATA_TYPE_PASSWORDS = DATA_TYPE_EMBEDDER_BEGIN << 2,
  DATA_TYPE_PLUGIN_DATA = DATA_TYPE_EMBEDDER_BEGIN << 3,
#if defined(OS_ANDROID)
  DATA_TYPE_WEB_APP_DATA = DATA_TYPE_EMBEDDER_BEGIN << 4,
#endif
  DATA_TYPE_SITE_USAGE_DATA = DATA_TYPE_EMBEDDER_BEGIN << 5,
  DATA_TYPE_DURABLE_PERMISSION = DATA_TYPE_EMBEDDER_BEGIN << 6,
  DATA_TYPE_EXTERNAL_PROTOCOL_DATA = DATA_TYPE_EMBEDDER_BEGIN << 7,
  DATA_TYPE_HOSTED_APP_DATA_TEST_ONLY = DATA_TYPE_EMBEDDER_BEGIN << 8,
  DATA_TYPE_CONTENT_SETTINGS = DATA_TYPE_EMBEDDER_BEGIN << 9,
  DATA_TYPE_BOOKMARKS = DATA_TYPE_EMBEDDER_BEGIN << 10,
  DATA_TYPE_ISOLATED_ORIGINS = DATA_TYPE_EMBEDDER_BEGIN << 11,
  DATA_TYPE_ACCOUNT_PASSWORDS = DATA_TYPE_EMBEDDER_BEGIN << 12,
  DATA_TYPE_LOCAL_CUSTOM_DICTIONARY = DATA_TYPE_EMBEDDER_BEGIN << 13,

  // Group datatypes.

  // "Site data" includes storage backend accessible to websites and some
  // additional metadata kept by the browser (e.g. site usage data).
  DATA_TYPE_SITE_DATA =
      content::BrowsingDataRemover::DATA_TYPE_COOKIES |
      content::BrowsingDataRemover::DATA_TYPE_DOM_STORAGE |
      content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES |
      DATA_TYPE_PLUGIN_DATA |
#if defined(OS_ANDROID)
      DATA_TYPE_WEB_APP_DATA |
#endif
      DATA_TYPE_SITE_USAGE_DATA | DATA_TYPE_DURABLE_PERMISSION |
      DATA_TYPE_EXTERNAL_PROTOCOL_DATA | DATA_TYPE_ISOLATED_ORIGINS |
      content::BrowsingDataRemover::DATA_TYPE_TRUST_TOKENS |
      content::BrowsingDataRemover::DATA_TYPE_CONVERSIONS,

  // Datatypes protected by Important Sites.
  IMPORTANT_SITES_DATA_TYPES =
      DATA_TYPE_SITE_DATA | content::BrowsingDataRemover::DATA_TYPE_CACHE,

  // Datatypes that can be deleted partially per URL / origin / domain,
  // whichever makes sense.
  FILTERABLE_DATA_TYPES = DATA_TYPE_SITE_DATA |
                          content::BrowsingDataRemover::DATA_TYPE_CACHE |
                          content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS,

  // Datatypes with account-scoped data that needs to be removed
  // before Google cookies are deleted.
  DEFERRED_COOKIE_DELETION_DATA_TYPES = DATA_TYPE_ACCOUNT_PASSWORDS,

  // Includes all the available remove options. Meant to be used by clients
  // that wish to wipe as much data as possible from a Profile, to make it
  // look like a new Profile. Does not delete account-scoped data like
  // passwords but will remove access to account-scoped data by signing the
  // user out.

  ALL_DATA_TYPES = DATA_TYPE_SITE_DATA |  //
                   content::BrowsingDataRemover::DATA_TYPE_CACHE |
                   content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS |
                   DATA_TYPE_FORM_DATA |         //
                   DATA_TYPE_HISTORY |           //
                   DATA_TYPE_PASSWORDS |         //
                   DATA_TYPE_CONTENT_SETTINGS |  //
                   DATA_TYPE_BOOKMARKS |         //
                   DATA_TYPE_LOCAL_CUSTOM_DICTIONARY,

  // Includes all available remove options. Meant to be used when the Profile
  // is scheduled to be deleted, and all possible data should be wiped from
  // disk as soon as possible.
  WIPE_PROFILE =
      ALL_DATA_TYPES | content::BrowsingDataRemover::DATA_TYPE_NO_CHECKS,
};

// This is an extension of content::BrowsingDataRemover::OriginType which
// includes all origin types therefrom and adds additional Chrome-specific
// ones.
enum OriginType : uint64_t {
  // Embedder can start adding origin types after the last
  // platform origin type.
  ORIGIN_TYPE_EMBEDDER_BEGIN =
      content::BrowsingDataRemover::ORIGIN_TYPE_CONTENT_END << 1,

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Packaged apps and extensions (chrome-extension://*).
  ORIGIN_TYPE_EXTENSION = ORIGIN_TYPE_EMBEDDER_BEGIN,
#endif

  // All origin types.
  ALL_ORIGIN_TYPES = content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
#if BUILDFLAG(ENABLE_EXTENSIONS)
                     ORIGIN_TYPE_EXTENSION |
#endif
                     content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
};

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
