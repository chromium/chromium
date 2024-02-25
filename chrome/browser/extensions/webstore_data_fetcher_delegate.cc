// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_data_fetcher_delegate.h"

namespace extensions {

const char WebstoreDataFetcherDelegate::kAverageRatingKey[] = "average_rating";
const char WebstoreDataFetcherDelegate::kIconUrlKey[] = "icon_url";
const char WebstoreDataFetcherDelegate::kIdKey[] = "id";
const char WebstoreDataFetcherDelegate::kLocalizedDescriptionKey[] =
    "localized_description";
const char WebstoreDataFetcherDelegate::kLocalizedNameKey[] = "localized_name";
const char WebstoreDataFetcherDelegate::kManifestKey[] = "manifest";
const char WebstoreDataFetcherDelegate::kRatingCountKey[] = "rating_count";
const char WebstoreDataFetcherDelegate::kShowUserCountKey[] = "show_user_count";
const char WebstoreDataFetcherDelegate::kUsersKey[] = "users";

const char WebstoreDataFetcherDelegate::kInvalidWebstoreResponseError[] =
    "Invalid Chrome Web Store response";

}  // namespace extensions
