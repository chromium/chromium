// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACTS_SORTER_H_
#define CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACTS_SORTER_H_

#include <vector>

#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"

// Sort |contacts| by the following fields:
//  - person name or email address if name is empty (primary),
//  - email, even if this is also used as the primary (secondary),
//  - phone number (tertiary),
//  - contact record id (last resort; should always be unique).
//
// This sorted order is unique for a given |locale|, presuming every element of
// |contacts| has a unique ContactRecord::id(). The ordering between fields is
// locale-dependent. For example, 'Å' will be sorted with these 'A's for
// US-based sorting, whereas 'Å' will be sorted after 'Z' for Sweden-based
// sorting, because 'Å' comes after 'Z' in the Swedish alphabet. By default,
// |locale| is inferred from system settings.
void SortNearbyShareContactRecords(
    std::vector<nearby::sharing::proto::ContactRecord>* contacts,
    icu::Locale locale = icu::Locale::getDefault());

#endif  // CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACTS_SORTER_H_
