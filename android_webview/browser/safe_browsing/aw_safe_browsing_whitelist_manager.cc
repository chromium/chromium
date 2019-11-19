// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/safe_browsing/aw_safe_browsing_whitelist_manager.h"

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/url_util.h"
#include "url/url_util.h"

namespace android_webview {

// This is a simple trie structure designed for handling host/domain matches
// for Safebrowsing whitelisting. For the match rules, see the class header.
//
// It is easy to visualize the trie edges as hostname components of a url in
// reverse order. For example a whitelist of google.com will have a tree
// tree structure as below.
//                       root
//                         | com
//                       Node1
//                google/    \ example
//                   Node2   Node3
//
// Normally, a search in the tree should end in a leaf node for a positive
// match. For example in the tree above com.google and com.example are matches.
// However, the whitelisting also allows matching subdomains if there is a
// leading dot,  for example, see ."google.com" and a.google.com below:
//                       root
//                         | com
//                       Node1
//                         | google
//                       Node2
//                         | a
//                       Node3
// Here, both Node2 and Node3 are valid terminal nodes to terminate a match.
// The boolean is_terminal indicates whether a node can successfully terminate
// a search (aka. whether this rule was entered to the whitelist) and
// match_prefix indicate if this should match exactly, or just do a prefix
// match.

// The structure is optimized such that if a node already allows a prefix
// match, then there is no need for it to have children, or if it has children
// these children are removed.
struct TrieNode {
  std::map<std::string, std::unique_ptr<TrieNode>> children;
  bool match_prefix = false;
  bool is_terminal = false;
};

namespace {

void InsertRuleToTrie(const std::vector<base::StringPiece>& components,
                      TrieNode* root,
                      bool match_prefix) {
  TrieNode* node = root;
  for (auto hostcomp = components.rbegin(); hostcomp != components.rend();
       ++hostcomp) {
    DCHECK(!node->match_prefix);
    std::string component = hostcomp->as_string();
    auto child_node = node->children.find(component);
    if (child_node == node->children.end()) {
      std::unique_ptr<TrieNode> temp = std::make_unique<TrieNode>();
      TrieNode* current = temp.get();
      node->children.emplace(component, std::move(temp));
      node = current;
    } else {
      node = child_node->second.get();
      // Optimization. No need to add new nodes as this node matches prefixes.
      DCHECK(node);
      if (node->match_prefix)
        return;
    }
  }
  DCHECK_NE(node, root);
  // Optimization. If match_prefix is true, remove all nodes originating
  // from that node.
  if (match_prefix) {
    node->match_prefix = true;
    node->children.clear();
  }
  node->is_terminal = true;
}

std::vector<base::StringPiece> SplitHost(const GURL& url) {
  std::vector<base::StringPiece> components;
  if (url.HostIsIPAddress()) {
    components.push_back(url.host_piece());
  } else {
    components =
        base::SplitStringPiece(url.host_piece(), ".", base::KEEP_WHITESPACE,
                               base::SPLIT_WANT_NONEMPTY);
  }
  DCHECK_GT(components.size(), 0u);
  return components;
}

// Rule is a UTF-8 wide string.
bool AddRuleToWhitelist(base::StringPiece rule, TrieNode* root) {
  if (rule.empty()) {
    return false;
  }
  // Leading dot means to do an exact match.
  bool started_with_dot = false;
  if (rule.front() == '.') {
    rule.remove_prefix(1);  // Strip it for the rest of the handling.
    started_with_dot = true;
  }
  // With the dot removed |rule| should look like a hostname.
  GURL test_url("http://" + rule.as_string());
  if (!test_url.is_valid()) {
    return false;
  }

  bool has_path = test_url.has_path() && test_url.path() != "/";
  // Verify that it is a hostname.
  if (!test_url.has_host() || has_path || test_url.has_port() ||
      test_url.has_query() || test_url.has_password() ||
      test_url.has_username() || test_url.has_ref()) {
    return false;
  }

  bool match_prefix = false;
  if (test_url.HostIsIPAddress()) {
    // leading dots are not allowed for IP addresses. IP addresses are always
    // exact match.
    if (started_with_dot) {
      return false;
    }
    match_prefix = false;
  } else {
    match_prefix = !started_with_dot;
  }
  InsertRuleToTrie(SplitHost(test_url), root, match_prefix);
  return true;
}

bool AddRules(const std::vector<std::string>& rules, TrieNode* root) {
  for (auto rule : rules) {
    if (!AddRuleToWhitelist(rule, root)) {
      LOG(ERROR) << " invalid whitelist rule " << rule;
      return false;
    }
  }
  return true;
}

bool IsWhitelisted(const GURL& url, const TrieNode* node) {
  std::vector<base::StringPiece> components = SplitHost(url);
  for (auto component = components.rbegin(); component != components.rend();
       ++component) {
    if (node->match_prefix) {
      return true;
    }
    auto child_node = node->children.find(component->as_string());
    if (child_node == node->children.end()) {
      return false;
    } else {
      node = child_node->second.get();
      DCHECK(node);
    }
  }
  // DCHECK optimization. A match_prefix node should have no children.
  DCHECK(!node->match_prefix || node->children.empty());
  // If trie search finished in a terminal node, host is found in trie. The
  // root node is not a terminal node.
  return node->is_terminal;
}

}  // namespace

AwSafeBrowsingWhitelistManager::AwSafeBrowsingWhitelistManager(
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner)
    : background_task_runner_(background_task_runner),
      io_task_runner_(io_task_runner),
      ui_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      whitelist_(std::make_unique<TrieNode>()) {}

AwSafeBrowsingWhitelistManager::~AwSafeBrowsingWhitelistManager() {}

void AwSafeBrowsingWhitelistManager::SetWhitelist(
    std::unique_ptr<TrieNode> whitelist) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  whitelist_ = std::move(whitelist);
}

// A task that builds the whitelist on a background thread.
void AwSafeBrowsingWhitelistManager::BuildWhitelist(
    const std::vector<std::string>& rules,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  std::unique_ptr<TrieNode> whitelist(std::make_unique<TrieNode>());
  bool success = AddRules(rules, whitelist.get());
  DCHECK(!whitelist->is_terminal);
  DCHECK(!whitelist->match_prefix);

  ui_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(std::move(callback), success));

  if (success) {
    // use base::Unretained as AwSafeBrowsingWhitelistManager is a singleton and
    // not cleaned.
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AwSafeBrowsingWhitelistManager::SetWhitelist,
                       base::Unretained(this), std::move(whitelist)));
  }
}

void AwSafeBrowsingWhitelistManager::SetWhitelistOnUIThread(
    std::vector<std::string>&& rules,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  // use base::Unretained as AwSafeBrowsingWhitelistManager is a singleton and
  // not cleaned.
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AwSafeBrowsingWhitelistManager::BuildWhitelist,
                                base::Unretained(this), std::move(rules),
                                std::move(callback)));
}

bool AwSafeBrowsingWhitelistManager::IsURLWhitelisted(const GURL& url) const {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (!url.has_host()) {
    return false;
  }
  return IsWhitelisted(url, whitelist_.get());
}

}  // namespace android_webview
