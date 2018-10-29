// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils.h"

#include <tuple>

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "url/gurl.h"

namespace extensions {

api::passwords_private::UrlCollection CreateUrlCollectionFromForm(
    const autofill::PasswordForm& form) {
  api::passwords_private::UrlCollection urls;
  GURL link_url;
  std::tie(urls.shown, link_url) =
      password_manager::GetShownOriginAndLinkUrl(form);
  urls.origin = form.signon_realm;
  urls.link = link_url.spec();
  return urls;
}

SortKeyIdGenerator::SortKeyIdGenerator() = default;

SortKeyIdGenerator::~SortKeyIdGenerator() = default;

int SortKeyIdGenerator::GenerateId(const std::string& sort_key) {
  auto result = sort_key_cache_.emplace(sort_key, next_id_);
  if (result.second) {
    // In case we haven't seen |sort_key| before, add a pointer to the inserted
    // key and the corresponding id to the |id_cache_|. This insertion should
    // always succeed.
    auto iter =
        id_cache_.emplace_hint(id_cache_.end(), next_id_, &result.first->first);
    DCHECK_EQ(&result.first->first, iter->second);
    ++next_id_;
  }

  return result.first->second;
}

const std::string* SortKeyIdGenerator::TryGetSortKey(int id) const {
  auto it = id_cache_.find(id);
  return it != id_cache_.end() ? it->second : nullptr;
}

}  // namespace extensions
