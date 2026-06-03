// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements a language matching algorithm that finds the "best"
// supported locale for a given preferred locale.
//
// The matching process relies on a precomputed graph where:
// 1. Nodes are LanguageTags.
// 2. Edges represent fallback relationships (e.g., "en-US" falls back to "en").
//    In the graph, these are stored as "parent to child" edges (e.g., "en" ->
//    "en-US") to facilitate finding supported descendants.
// 3. Supported locales have a distance of 0 to themselves.
// 4. Non-supported ancestors (like "en" or "es") precompute their distance to
//     the "closest" supported locale using a shortest-path (DFS) approach.
//
// Shortest paths are influenced by "edge weights". A default weight of 1.0 is
// used, but specific pairs (e.g., "es-419" -> "es-MX") can have lower weights
// to express a stronger preference for that specific mapping.

#include "base/i18n/language_tag_matcher.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/i18n/internal/icu_bridge.rs.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "base/i18n/tags.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace base {
namespace {

using ::base::i18n::internal::create_icu_fallbacker;
using ::base::i18n::internal::Icu4xLocale;
using ::base::i18n::internal::IcuFallbacker;

// Returns the sequence of fallback locales using ICU4X logic, excluding the
// original locale and the root locale ("und").
//
// Examples:
// - "en-US" -> ["en"]
// - "es-AR" -> ["es-419", "es"]
// - "zh-TW" -> ["zh-Hant"]
std::vector<LanguageTag> GetFallbackLocales(const IcuFallbacker& icu_fallbacker,
                                            const LanguageTag& locale) {
  rust::Vec<Icu4xLocale> fallback_locales =
      icu_fallbacker.fallback_to_vec(rust::Slice<const uint8_t>(
          reinterpret_cast<const uint8_t*>(locale.ToString().data()),
          locale.ToString().size()));
  if (fallback_locales.empty()) {
    return {};
  }

  const auto& ltag_builder = LanguageTagConverter::GetInstance();

  // ICU4X's fallbacker normalizes the locale striping away default scripts for
  // languages when they are present, this could make the original locale not
  // appear in the output.
  LanguageTag first_fallback_locale =
      ltag_builder.FromIcu4xLocale(fallback_locales.front());
  std::vector<LanguageTag> result;
  result.reserve(fallback_locales.size());
  if (first_fallback_locale != locale) {
    result.push_back(first_fallback_locale);
  }

  // Skip the first locale in the fallback chain as it is the original locale.
  for (size_t i = 1; i < fallback_locales.size(); ++i) {
    result.push_back(ltag_builder.FromIcu4xLocale(fallback_locales[i]));
  }

  return result;
}

// Returns the weight of the edge between a source locale and its target
// (more specific) descendant. Lower weights indicate a stronger preference
// for that mapping.
// Non-default weights are being used because matching such as 'en' -> 'en-US',
// are a requirement. Another example is when there is multiple Latin American
// Spanish locales and the preferred one is es-MX. Once This would no longer be
// needed when ICU4X implements a locale matcher:
// https://github.com/unicode-org/icu4x/issues/3023
float GetEdgeWeight(const LanguageTag& source, const LanguageTag& target) {
  static const base::NoDestructor<
      base::flat_map<std::pair<LanguageTag, LanguageTag>, float>>
      kNonDefaultEdges([]() {
        base::flat_map<std::pair<LanguageTag, LanguageTag>, float>
            non_default_edges;
        // Prefer Mexican Spanish for Latin American Spanish.
        non_default_edges.emplace(
            std::make_pair(language_tags::SPANISH_LATIN_AMERICAN(),
                           language_tags::SPANISH_MEXICO()),
            0.8);
        // Prefer Brazilian Portuguese for generic Portuguese.
        non_default_edges.emplace(
            std::make_pair(language_tags::PORTUGUESE(),
                           language_tags::BRAZILIAN_PORTUGUESE()),
            0.8);
        // Global English (en-001) preferences.
        LanguageTag english_global =
            LanguageTagConverter::GetInstance().FromString("en-001").value();
        non_default_edges.emplace(
            std::make_pair(english_global, language_tags::ENGLISH_US()), 0.8);
        non_default_edges.emplace(
            std::make_pair(english_global, language_tags::BRITISH_ENGLISH()),
            0.81);
        // Chinese locales
        non_default_edges.emplace(
            std::make_pair(language_tags::CHINESE(),
                           language_tags::CHINA_CHINESE()),
            0.8);

        return non_default_edges;
      }());

  if (auto it = kNonDefaultEdges->find({source, target});
      it != kNonDefaultEdges->end()) {
    return it->second;
  }
  return 1.0;
}

// A graph used to precompute the best supported locale for various language
// tags. It builds a directed graph where edges point from a parent locale
// (e.g., "en") to a child locale (e.g., "en-US").
class LanguageTagPreferenceGraph {
 public:
  LanguageTagPreferenceGraph(const IcuFallbacker& icu_fallbacker,
                             base::span<const LanguageTag> supported_tags) {
    for (const LanguageTag& supported_tag : supported_tags) {
      // Build the graph by tracing the fallback chain of each supported locale.
      // If a supported locale is "zh-Hans-CN", its fallbacks might be:
      // zh-Hans-CN -> [zh-Hans,  zh].
      // Edges are added:
      // zh -> zh-Hans -> zh-Hans-CN.
      // This allows traversing from a generic locale to the most specific
      // supported one.
      LanguageTag previous = supported_tag;
      for (const LanguageTag& fallback_tag :
           GetFallbackLocales(icu_fallbacker, supported_tag)) {
        // For example, an edge is added between <en -> en-US>
        edges_[fallback_tag].push_back(previous);
        distance_.try_emplace(fallback_tag, std::numeric_limits<float>::max());
        previous = fallback_tag;
      }

      // Supported locales are their own best match with 0 distance.
      distance_.insert_or_assign(supported_tag, 0);
      closest_supported_tag_.insert_or_assign(supported_tag, supported_tag);
    }
  }

  // Computes the closest supported locale for all reachable nodes in the graph.
  // A vector is returned because an immutable flat_map can be efficiently
  // created from a vector, which just needs to be sorted.
  base::flat_map<LanguageTag, LanguageTag> ComputeClosestSupportedTag() && {
    // Traverse the graph starting from every root/ancestor node to precompute
    // the best supported descendant.
    // The distance_ map has an entry for every node in the graph initialized
    // with a high value for non-supported locales and 0 for supported locales.
    for (auto& it : distance_) {
      Dfs(it.first);
    }

    std::vector<std::pair<LanguageTag, LanguageTag>> output;
    output.reserve(closest_supported_tag_.size());
    std::ranges::move(closest_supported_tag_.begin(),
                      closest_supported_tag_.end(), std::back_inserter(output));
    return base::flat_map(std::move(output));
  }

 private:
  struct Result {
    float distance;
    LanguageTag closest_supported;
  };

  // Uses memoized DFS to find the shortest path from 'current' to any
  // supported locale.
  Result Dfs(const LanguageTag& current) {
    auto it = distance_.find(current);
    auto it_closest_supported = closest_supported_tag_.find(current);
    // If this node has already been computed (or it's a supported locale),
    // return the cached result.
    if (it != distance_.end() &&
        it_closest_supported != closest_supported_tag_.end()) {
      return Result{.distance = it->second,
                    .closest_supported = it_closest_supported->second};
    }

    Result best_result = {.distance = std::numeric_limits<float>::max(),
                          .closest_supported = current};
    // Explore all child locales to find the one that leads to a supported
    // locale with the minimum total weight.
    auto edges_it = edges_.find(current);
    if (edges_it == edges_.end()) {
      return best_result;
    }

    for (const LanguageTag& next_locale : edges_it->second) {
      Result result = Dfs(next_locale);
      if (result.distance + GetEdgeWeight(current, next_locale) <
          best_result.distance) {
        best_result.distance =
            result.distance + GetEdgeWeight(current, next_locale);
        best_result.closest_supported = result.closest_supported;
      }
    }

    // Cache the result for this node.
    distance_.insert_or_assign(current, best_result.distance);
    closest_supported_tag_.insert_or_assign(current,
                                            best_result.closest_supported);
    return best_result;
  }

  absl::flat_hash_map<LanguageTag, std::vector<LanguageTag>> edges_;
  absl::flat_hash_map<LanguageTag, LanguageTag> closest_supported_tag_;
  absl::flat_hash_map<LanguageTag, float> distance_;
};

}  // namespace

// static
LanguageTagMatcher LanguageTagMatcher::Create(
    base::span<const LanguageTag> supported_tags) {
  rust::Box<IcuFallbacker> fallbacker = create_icu_fallbacker();
  LanguageTagPreferenceGraph graph(*fallbacker, supported_tags);

  return LanguageTagMatcher(std::move(graph).ComputeClosestSupportedTag(),
                            std::move(fallbacker));
}

std::optional<LanguageTag> LanguageTagMatcher::Match(
    const LanguageTag& preferred_locale) const {
  // Step 1: Check if the preferred locale or any of its ancestors were part of
  // the precomputed graph (i.e., they are supported or lead to a supported
  // locale).
  auto it = closest_supported_tag_.find(preferred_locale);
  if (it != closest_supported_tag_.end()) {
    return it->second;
  }

  // Step 2: If the preferred locale is not in the graph, traverse its fallback
  // chain and check each ancestor. For example, if "fr-CA" is preferred but
  // not in the graph, its fallbacks ("fr", "und") are checked.
  for (const LanguageTag& fallback :
       GetFallbackLocales(*icu_fallbacker_, preferred_locale)) {
    auto it_fallback = closest_supported_tag_.find(fallback);
    if (it_fallback != closest_supported_tag_.end()) {
      return it_fallback->second;
    }
  }

  return std::nullopt;
}

LanguageTagMatcher::LanguageTagMatcher(
    base::flat_map<LanguageTag, LanguageTag> closest_supported_tag,
    rust::Box<i18n::internal::IcuFallbacker> icu_fallbacker)
    : closest_supported_tag_(std::move(closest_supported_tag)),
      icu_fallbacker_(std::move(icu_fallbacker)) {}

LanguageTagMatcher::~LanguageTagMatcher() = default;

}  // namespace base
