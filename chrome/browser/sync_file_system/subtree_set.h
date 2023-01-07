// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SUBTREE_SET_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SUBTREE_SET_H_

#include <stddef.h>

#include <unordered_map>

#include "base/files/file_path.h"

namespace base {
class FilePath;
}  // namespace base

namespace sync_file_system {

// Stores disjoint subtrees of a directory tree.
class SubtreeSet {
 public:
  SubtreeSet();
  SubtreeSet(const SubtreeSet& other);
  ~SubtreeSet();

  // Returns true if the subtree induced by |subtree_root| is disjoint with
  // all subtrees in the container.
  bool IsDisjointWith(const base::FilePath& subtree_root) const;

  // Returns true and inserts the subtree induced by |subtree_root| if the
  // subtree is disjoint with all subtrees in the container.
  bool insert(const base::FilePath& subtree_root);

  // Erases the subtree induced by |subtree_root| from the container.
  // Returns true if this erases the subtree.
  bool erase(const base::FilePath& subtree_root);

  size_t size() const;
  bool empty() const { return inclusive_ancestors_of_subtree_roots_.empty(); }

 private:
  struct Node {
    bool contained_as_subtree_root;
    size_t number_of_subtrees_below;

    Node();
    Node(bool contained_as_subtree_root,
         size_t number_of_subtrees_below);
  };

  typedef base::FilePath::StringType StringType;
  typedef std::unordered_map<StringType, Node> Subtrees;

  // Contains the root of subtrees and all upward node to root.
  // Each subtree root has |contained_as_subtree_root| flag true.
  Subtrees inclusive_ancestors_of_subtree_roots_;
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SUBTREE_SET_H_
