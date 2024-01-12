// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/file_util.h"

#include <string>

#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"

namespace policy {

namespace {

// The file system APIs error out if we create a directory whose name exceeds
// this.
// TODO(b/300078530): I manually tested this limit and it seems accurate at the
// time of writing, but if we use `base::GetMaximumPathComponentLength` then
// there is no need to hard-code anything. However that will require refactoring
// callers to use a callback, since `base::GetMaximumPathComponentLength` is
// blocking and must be used async.
constexpr int kMaxPathComponentLength = 255;

bool ExceedsMaximumPathComponentLength(const std::string& component) {
  return component.size() > kMaxPathComponentLength;
}

}  // namespace

std::string GetUniqueSubDirectoryForAccountID(const std::string& account_id) {
  std::string sub_directory = base::HexEncode(base::as_byte_span(account_id));
  if (ExceedsMaximumPathComponentLength(sub_directory)) {
    // Originally the hex encoded account id was used as sub-directory name,
    // so we must keep on using that to guarantee backwards compatibility.
    // However this fails for long account-ids (since the filesystem has a limit
    // to how long a simple path component can be), in which case we switch to
    // hashes.
    return base::StringPrintf("%u", base::PersistentHash(account_id));
  }
  return sub_directory;
}

}  // namespace policy
