// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/subtree_set.h"

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "storage/common/file_system/file_system_util.h"

namespace sync_file_system {

SubtreeSet::Node::Node()
    : contained_as_subtree_root(false),
      number_of_subtrees_below(0) {
}

SubtreeSet::Node::Node(bool contained_as_subtree_root,
                       size_t number_of_subtrees_below)
    : contained_as_subtree_root(contained_as_subtree_root),
      number_of_subtrees_below(number_of_subtrees_below) {
}

SubtreeSet::SubtreeSet() {}
SubtreeSet::SubtreeSet(const SubtreeSet& other) = default;
SubtreeSet::~SubtreeSet() {}

bool SubtreeSet::IsDisjointWith(const base::FilePath& subtree_root) const {
  base::FilePath::StringType normalized_subtree_root =
      storage::VirtualPath::GetNormalizedFilePath(subtree_root);

  // Check if |subtree_root| contains any of subtrees in the container.
  if (base::Contains(inclusive_ancestors_of_subtree_roots_,
                     normalized_subtree_root))
    return false;

  base::FilePath path(normalized_subtree_root);
  while (!storage::VirtualPath::IsRootPath(path)) {
    path = storage::VirtualPath::DirName(path);

    auto found = inclusive_ancestors_of_subtree_roots_.find(path.value());
    if (found != inclusive_ancestors_of_subtree_roots_.end())
      return !found->second.contained_as_subtree_root;
  }

  return true;
}

bool SubtreeSet::insert(const base::FilePath& subtree_root) {
  base::FilePath::StringType normalized_subtree_root =
      storage::VirtualPath::GetNormalizedFilePath(subtree_root);

  if (!IsDisjointWith(subtree_root))
    return false;
  inclusive_ancestors_of_subtree_roots_[normalized_subtree_root]
      = Node(true, 1);

  base::FilePath path(normalized_subtree_root);
  while (!storage::VirtualPath::IsRootPath(path)) {
    path = storage::VirtualPath::DirName(path);
    DCHECK(!inclusive_ancestors_of_subtree_roots_[path.value()]
                .contained_as_subtree_root);
    ++(inclusive_ancestors_of_subtree_roots_[path.value()]
           .number_of_subtrees_below);
  }

  return true;
}

bool SubtreeSet::erase(const base::FilePath& subtree_root) {
  base::FilePath::StringType normalized_subtree_root =
      storage::VirtualPath::GetNormalizedFilePath(subtree_root);

  {
    auto found =
        inclusive_ancestors_of_subtree_roots_.find(normalized_subtree_root);
    if (found == inclusive_ancestors_of_subtree_roots_.end() ||
        !found->second.contained_as_subtree_root)
      return false;

    DCHECK_EQ(1u, found->second.number_of_subtrees_below);
    inclusive_ancestors_of_subtree_roots_.erase(found);
  }

  base::FilePath path(normalized_subtree_root);
  while (!storage::VirtualPath::IsRootPath(path)) {
    path = storage::VirtualPath::DirName(path);

    auto found = inclusive_ancestors_of_subtree_roots_.find(path.value());
    if (found == inclusive_ancestors_of_subtree_roots_.end()) {
      NOTREACHED_IN_MIGRATION();
      continue;
    }

    DCHECK(!found->second.contained_as_subtree_root);
    if (!--(found->second.number_of_subtrees_below))
      inclusive_ancestors_of_subtree_roots_.erase(found);
  }

  return true;
}

size_t SubtreeSet::size() const {
  auto found =
      inclusive_ancestors_of_subtree_roots_.find(storage::VirtualPath::kRoot);
  if (found == inclusive_ancestors_of_subtree_roots_.end())
    return 0;
  return found->second.number_of_subtrees_below;
}

}  // namespace sync_file_system
