// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/substring_set_matcher/substring_set_matcher.h"

#include <stddef.h>

#include <algorithm>
#include <queue>

#ifdef __SSE2__
#include <immintrin.h>
#include "base/bits.h"
#endif

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/queue.h"
#include "base/numerics/checked_math.h"
#include "base/trace_event/memory_usage_estimator.h"  // no-presubmit-check

namespace base {

namespace {

// Compare MatcherStringPattern instances based on their string patterns.
bool ComparePatterns(const MatcherStringPattern* a,
                     const MatcherStringPattern* b) {
  return a->pattern() < b->pattern();
}

std::vector<const MatcherStringPattern*> GetVectorOfPointers(
    const std::vector<MatcherStringPattern>& patterns) {
  std::vector<const MatcherStringPattern*> pattern_pointers;
  pattern_pointers.reserve(patterns.size());

  for (const MatcherStringPattern& pattern : patterns)
    pattern_pointers.push_back(&pattern);

  return pattern_pointers;
}

}  // namespace

bool SubstringSetMatcher::Build(
    const std::vector<MatcherStringPattern>& patterns) {
  return Build(GetVectorOfPointers(patterns));
}

bool SubstringSetMatcher::Build(
    std::vector<const MatcherStringPattern*> patterns) {
  // Ensure there are no duplicate IDs and all pattern strings are distinct.
#if DCHECK_IS_ON()
  {
    std::set<MatcherStringPattern::ID> ids;
    std::set<std::string> pattern_strings;
    for (const MatcherStringPattern* pattern : patterns) {
      CHECK(!base::Contains(ids, pattern->id()));
      CHECK(!base::Contains(pattern_strings, pattern->pattern()));
      ids.insert(pattern->id());
      pattern_strings.insert(pattern->pattern());
    }
  }
#endif

  // Check that all the match labels fit into an edge.
  for (const MatcherStringPattern* pattern : patterns) {
    if (pattern->id() >= kInvalidNodeID) {
      return false;
    }
  }

  // Compute the total number of tree nodes needed.
  std::sort(patterns.begin(), patterns.end(), ComparePatterns);
  NodeID tree_size = GetTreeSize(patterns);
  if (tree_size >= kInvalidNodeID) {
    return false;
  }
  tree_.reserve(GetTreeSize(patterns));
  BuildAhoCorasickTree(patterns);

  // Sanity check that no new allocations happened in the tree and our computed
  // size was correct.
  DCHECK_EQ(tree_.size(), static_cast<size_t>(GetTreeSize(patterns)));

  is_empty_ = patterns.empty() && tree_.size() == 1u;
  return true;
}

SubstringSetMatcher::SubstringSetMatcher() = default;
SubstringSetMatcher::~SubstringSetMatcher() = default;

bool SubstringSetMatcher::Match(
    const std::string& text,
    std::set<MatcherStringPattern::ID>* matches) const {
  const size_t old_number_of_matches = matches->size();

  // Handle patterns matching the empty string.
  const AhoCorasickNode* const root = &tree_[kRootID];
  AccumulateMatchesForNode(root, matches);

  const AhoCorasickNode* current_node = root;
  for (const char c : text) {
    NodeID child = current_node->GetEdge(static_cast<unsigned char>(c));

    // If the child not can't be found, progressively iterate over the longest
    // proper suffix of the string represented by the current node. In a sense
    // we are pruning prefixes from the text.
    while (child == kInvalidNodeID && current_node != root) {
      current_node = &tree_[current_node->failure()];
      child = current_node->GetEdge(static_cast<unsigned char>(c));
    }

    if (child != kInvalidNodeID) {
      // The string represented by |child| is the longest possible suffix of the
      // current position of |text| in the trie.
      current_node = &tree_[child];
      AccumulateMatchesForNode(current_node, matches);
    } else {
      // The empty string is the longest possible suffix of the current position
      // of |text| in the trie.
      DCHECK_EQ(root, current_node);
    }
  }

  return old_number_of_matches != matches->size();
}

bool SubstringSetMatcher::AnyMatch(const std::string& text) const {
  // Handle patterns matching the empty string.
  const AhoCorasickNode* const root = &tree_[kRootID];
  if (root->has_outputs()) {
    return true;
  }

  const AhoCorasickNode* current_node = root;
  for (const char c : text) {
    NodeID child = current_node->GetEdge(static_cast<unsigned char>(c));

    // If the child not can't be found, progressively iterate over the longest
    // proper suffix of the string represented by the current node. In a sense
    // we are pruning prefixes from the text.
    while (child == kInvalidNodeID && current_node != root) {
      current_node = &tree_[current_node->failure()];
      child = current_node->GetEdge(static_cast<unsigned char>(c));
    }

    if (child != kInvalidNodeID) {
      // The string represented by |child| is the longest possible suffix of the
      // current position of |text| in the trie.
      current_node = &tree_[child];
      if (current_node->has_outputs()) {
        return true;
      }
    } else {
      // The empty string is the longest possible suffix of the current position
      // of |text| in the trie.
      DCHECK_EQ(root, current_node);
    }
  }

  return false;
}

size_t SubstringSetMatcher::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(tree_);
}

// static
constexpr SubstringSetMatcher::NodeID SubstringSetMatcher::kInvalidNodeID;
constexpr SubstringSetMatcher::NodeID SubstringSetMatcher::kRootID;

SubstringSetMatcher::NodeID SubstringSetMatcher::GetTreeSize(
    const std::vector<const MatcherStringPattern*>& patterns) const {
  DCHECK(std::is_sorted(patterns.begin(), patterns.end(), ComparePatterns));

  base::CheckedNumeric<NodeID> result = 1u;  // 1 for the root node.
  if (patterns.empty())
    return result.ValueOrDie();

  auto last = patterns.begin();
  auto current = last + 1;
  // For the first pattern, each letter is a label of an edge to a new node.
  result += (*last)->pattern().size();

  // For the subsequent patterns, only count the edges which were not counted
  // yet. For this it suffices to test against the previous pattern, because the
  // patterns are sorted.
  for (; current != patterns.end(); ++last, ++current) {
    const std::string& last_pattern = (*last)->pattern();
    const std::string& current_pattern = (*current)->pattern();
    size_t prefix_bound = std::min(last_pattern.size(), current_pattern.size());

    size_t common_prefix = 0;
    while (common_prefix < prefix_bound &&
           last_pattern[common_prefix] == current_pattern[common_prefix]) {
      ++common_prefix;
    }

    result -= common_prefix;
    result += current_pattern.size();
  }

  return result.ValueOrDie();
}

void SubstringSetMatcher::BuildAhoCorasickTree(
    const SubstringPatternVector& patterns) {
  DCHECK(tree_.empty());

  // Initialize root node of tree.
  tree_.emplace_back();

  // Build the initial trie for all the patterns.
  for (const MatcherStringPattern* pattern : patterns)
    InsertPatternIntoAhoCorasickTree(pattern);

  CreateFailureAndOutputEdges();
}

void SubstringSetMatcher::InsertPatternIntoAhoCorasickTree(
    const MatcherStringPattern* pattern) {
  const std::string& text = pattern->pattern();
  const std::string::const_iterator text_end = text.end();

  // Iterators on the tree and the text.
  AhoCorasickNode* current_node = &tree_[kRootID];
  std::string::const_iterator i = text.begin();

  // Follow existing paths for as long as possible.
  while (i != text_end) {
    NodeID child = current_node->GetEdge(static_cast<unsigned char>(*i));
    if (child == kInvalidNodeID)
      break;
    current_node = &tree_[child];
    ++i;
  }

  // Create new nodes if necessary.
  while (i != text_end) {
    tree_.emplace_back();
    current_node->SetEdge(static_cast<unsigned char>(*i),
                          static_cast<NodeID>(tree_.size() - 1));
    current_node = &tree_.back();
    ++i;
  }

  // Register match.
  current_node->SetMatchID(pattern->id());
}

void SubstringSetMatcher::CreateFailureAndOutputEdges() {
  base::queue<AhoCorasickNode*> queue;

  // Initialize the failure edges for |root| and its children.
  AhoCorasickNode* const root = &tree_[0];

  root->SetOutputLink(kInvalidNodeID);

  NodeID root_output_link = root->IsEndOfPattern() ? kRootID : kInvalidNodeID;

  for (unsigned edge_idx = 0; edge_idx < root->num_edges(); ++edge_idx) {
    const AhoCorasickEdge& edge = root->edges()[edge_idx];
    if (edge.label >= kFirstSpecialLabel) {
      continue;
    }
    AhoCorasickNode* child = &tree_[edge.node_id];
    // Failure node is kept as the root.
    child->SetOutputLink(root_output_link);
    queue.push(child);
  }

  // Do a breadth first search over the trie to create failure edges. We
  // maintain the invariant that any node in |queue| has had its |failure_| and
  // |output_link_| edge already initialized.
  while (!queue.empty()) {
    AhoCorasickNode* current_node = queue.front();
    queue.pop();

    // Compute the failure and output edges of children using the failure edges
    // of the current node.
    for (unsigned edge_idx = 0; edge_idx < current_node->num_edges();
         ++edge_idx) {
      const AhoCorasickEdge& edge = current_node->edges()[edge_idx];
      if (edge.label >= kFirstSpecialLabel) {
        continue;
      }
      AhoCorasickNode* child = &tree_[edge.node_id];

      const AhoCorasickNode* failure_candidate_parent =
          &tree_[current_node->failure()];
      NodeID failure_candidate_id =
          failure_candidate_parent->GetEdge(edge.label);
      while (failure_candidate_id == kInvalidNodeID &&
             failure_candidate_parent != root) {
        failure_candidate_parent = &tree_[failure_candidate_parent->failure()];
        failure_candidate_id = failure_candidate_parent->GetEdge(edge.label);
      }

      if (failure_candidate_id == kInvalidNodeID) {
        DCHECK_EQ(root, failure_candidate_parent);
        // |failure_candidate| is invalid and we can't proceed further since we
        // have reached the root. Hence the longest proper suffix of this string
        // represented by this node is the empty string (represented by root).
        failure_candidate_id = kRootID;
      } else {
        child->SetFailure(failure_candidate_id);
      }

      const AhoCorasickNode* failure_candidate = &tree_[failure_candidate_id];
      // Now |failure_candidate| is |child|'s longest possible proper suffix in
      // the trie. We also know that since we are doing a breadth first search,
      // we would have established |failure_candidate|'s output link by now.
      // Hence we can define |child|'s output link as follows:
      child->SetOutputLink(failure_candidate->IsEndOfPattern()
                               ? failure_candidate_id
                               : failure_candidate->output_link());

      queue.push(child);
    }
  }
}

void SubstringSetMatcher::AccumulateMatchesForNode(
    const AhoCorasickNode* node,
    std::set<MatcherStringPattern::ID>* matches) const {
  DCHECK(matches);

  if (!node->has_outputs()) {
    // Fast reject.
    return;
  }
  if (node->IsEndOfPattern())
    matches->insert(node->GetMatchID());

  NodeID node_id = node->output_link();
  while (node_id != kInvalidNodeID) {
    node = &tree_[node_id];
    matches->insert(node->GetMatchID());
    node_id = node->output_link();
  }
}

SubstringSetMatcher::AhoCorasickNode::AhoCorasickNode() {
  static_assert(kNumInlineEdges == 2, "Code below needs updating");
  edges_.inline_edges[0].label = kEmptyLabel;
  edges_.inline_edges[1].label = kEmptyLabel;
}

SubstringSetMatcher::AhoCorasickNode::~AhoCorasickNode() {
  if (edges_capacity_ != 0) {
    delete[] edges_.edges;
  }
}

SubstringSetMatcher::AhoCorasickNode::AhoCorasickNode(AhoCorasickNode&& other) {
  *this = std::move(other);
}

SubstringSetMatcher::AhoCorasickNode&
SubstringSetMatcher::AhoCorasickNode::operator=(AhoCorasickNode&& other) {
  if (edges_capacity_ != 0) {
    // Delete the old heap allocation if needed.
    delete[] edges_.edges;
  }
  if (other.edges_capacity_ == 0) {
    static_assert(kNumInlineEdges == 2, "Code below needs updating");
    edges_.inline_edges[0] = other.edges_.inline_edges[0];
    edges_.inline_edges[1] = other.edges_.inline_edges[1];
  } else {
    // Move over the heap allocation.
    edges_.edges = other.edges_.edges;
    other.edges_.edges = nullptr;
  }
  num_free_edges_ = other.num_free_edges_;
  edges_capacity_ = other.edges_capacity_;
  return *this;
}

SubstringSetMatcher::NodeID
SubstringSetMatcher::AhoCorasickNode::GetEdgeNoInline(uint32_t label) const {
  DCHECK(edges_capacity_ != 0);
#ifdef __SSE2__
  const __m128i lbl = _mm_set1_epi32(static_cast<int>(label));
  const __m128i mask = _mm_set1_epi32(0x1ff);
  for (unsigned edge_idx = 0; edge_idx < num_edges(); edge_idx += 4) {
    const __m128i four = _mm_loadu_si128(
        reinterpret_cast<const __m128i*>(&edges_.edges[edge_idx]));
    const __m128i match = _mm_cmpeq_epi32(_mm_and_si128(four, mask), lbl);
    const uint32_t match_mask = static_cast<uint32_t>(_mm_movemask_epi8(match));
    if (match_mask != 0) {
      if (match_mask & 0x1u) {
        return edges_.edges[edge_idx].node_id;
      }
      if (match_mask & 0x10u) {
        return edges_.edges[edge_idx + 1].node_id;
      }
      if (match_mask & 0x100u) {
        return edges_.edges[edge_idx + 2].node_id;
      }
      DCHECK(match_mask & 0x1000u);
      return edges_.edges[edge_idx + 3].node_id;
    }
  }
#else
  for (unsigned edge_idx = 0; edge_idx < num_edges(); ++edge_idx) {
    const AhoCorasickEdge& edge = edges_.edges[edge_idx];
    if (edge.label == label)
      return edge.node_id;
  }
#endif
  return kInvalidNodeID;
}

void SubstringSetMatcher::AhoCorasickNode::SetEdge(uint32_t label,
                                                   NodeID node) {
  DCHECK_LT(node, kInvalidNodeID);

#if DCHECK_IS_ON()
  // We don't support overwriting existing edges.
  for (unsigned edge_idx = 0; edge_idx < num_edges(); ++edge_idx) {
    DCHECK_NE(label, edges()[edge_idx].label);
  }
#endif

  if (edges_capacity_ == 0 && num_free_edges_ > 0) {
    // Still space in the inline storage, so use that.
    edges_.inline_edges[num_edges()] = AhoCorasickEdge{label, node};
    if (label == kFailureNodeLabel) {
      // Make sure that kFailureNodeLabel is first.
      // NOTE: We don't use std::swap here, because the compiler doesn't
      // understand that inline_edges[] is 4-aligned and can give
      // a warning or error.
      AhoCorasickEdge temp = edges_.inline_edges[0];
      edges_.inline_edges[0] = edges_.inline_edges[num_edges()];
      edges_.inline_edges[num_edges()] = temp;
    }
    --num_free_edges_;
    return;
  }

  if (num_free_edges_ == 0) {
    // We are out of space, so double our capacity (unless that would cause
    // num_free_edges_ to overflow). This can either be because we are
    // converting from inline to heap storage, or because we are increasing the
    // size of our heap storage.
    unsigned old_capacity =
        edges_capacity_ == 0 ? kNumInlineEdges : edges_capacity_;
    unsigned new_capacity = std::min(old_capacity * 2, kEmptyLabel + 1);
    DCHECK_EQ(0u, new_capacity % 4);
    AhoCorasickEdge* new_edges = new AhoCorasickEdge[new_capacity];
    memcpy(new_edges, edges(), sizeof(AhoCorasickEdge) * old_capacity);
    for (unsigned edge_idx = old_capacity; edge_idx < new_capacity;
         ++edge_idx) {
      new_edges[edge_idx].label = kEmptyLabel;
    }
    if (edges_capacity_ != 0) {
      delete[] edges_.edges;
    }
    edges_.edges = new_edges;
    // These casts are safe due to the DCHECK above.
    edges_capacity_ = static_cast<uint16_t>(new_capacity);
    num_free_edges_ = static_cast<uint8_t>(new_capacity - old_capacity);
  }

  // Insert the new edge at the end of our heap storage.
  edges_.edges[num_edges()] = AhoCorasickEdge{label, node};
  if (label == kFailureNodeLabel) {
    // Make sure that kFailureNodeLabel is first.
    std::swap(edges_.edges[0], edges_.edges[num_edges()]);
  }
  --num_free_edges_;
}

void SubstringSetMatcher::AhoCorasickNode::SetFailure(NodeID node) {
  DCHECK_NE(kInvalidNodeID, node);
  if (node != kRootID) {
    SetEdge(kFailureNodeLabel, node);
  }
}

size_t SubstringSetMatcher::AhoCorasickNode::EstimateMemoryUsage() const {
  if (edges_capacity_ == 0) {
    return 0;
  } else {
    return base::trace_event::EstimateMemoryUsage(
        base::span<const AhoCorasickEdge>(edges_.edges, edges_capacity_));
  }
}

}  // namespace base
