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
#include "base/containers/span.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "base/no_destructor.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/rust/chromium_crates_io/vendor/icu_capi-v2/bindings/cpp/icu4x/Locale.hpp"
#include "third_party/rust/chromium_crates_io/vendor/icu_capi-v2/bindings/cpp/icu4x/LocaleFallbackIterator.hpp"
#include "third_party/rust/chromium_crates_io/vendor/icu_capi-v2/bindings/cpp/icu4x/LocaleFallbacker.hpp"
#include "third_party/rust/chromium_crates_io/vendor/icu_capi-v2/bindings/cpp/icu4x/LocaleFallbackerWithConfig.hpp"

namespace base::i18n {
namespace {

// Returns the sequence of fallback locales using ICU4X logic, excluding the
// original locale and the root locale ("und").
//
// Examples:
// - "en-US" -> ["en"]
// - "es-AR" -> ["es-419", "es"]
// - "zh-TW" -> ["zh-Hant"]
std::vector<LanguageTag> GetFallbackLocales(
    const icu4x::LocaleFallbacker* icu_fallbacker,
    const LanguageTag& locale) {
  auto parse_res = icu4x::Locale::from_string(locale.tag_string());
  if (!parse_res.is_ok()) {
    return {};
  }
  std::unique_ptr<icu4x::Locale> locale_ptr = std::move(parse_res).ok().value();

  icu4x::LocaleFallbackConfig config = {
      icu4x::LocaleFallbackPriority::Language};
  std::unique_ptr<icu4x::LocaleFallbackerWithConfig> fallbacker_with_config =
      icu_fallbacker->for_config(config);
  std::unique_ptr<icu4x::LocaleFallbackIterator> iter =
      fallbacker_with_config->fallback_for_locale(*locale_ptr);

  std::vector<LanguageTag> fallback_locales;
  const auto& ltag_builder = LanguageTagConverter::GetInstance();

  while (std::unique_ptr<icu4x::Locale> fallback_locale = iter->next()) {
    fallback_locales.push_back(
        ltag_builder.FromIcu4xCapiLocale(*fallback_locale));
  }

  if (fallback_locales.empty()) {
    return {};
  }

  LanguageTag first_fallback_locale = fallback_locales.front();
  std::vector<LanguageTag> result;
  result.reserve(fallback_locales.size());
  if (first_fallback_locale != locale) {
    result.push_back(first_fallback_locale);
  }

  for (size_t i = 1; i < fallback_locales.size(); ++i) {
    result.push_back(fallback_locales[i]);
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
            non_default_edges{
                {{GetKnownLanguageTag("es-419"), GetKnownLanguageTag("es-MX")},
                 0.8},
                // For english global (en-001), we favor "en-GB" matches by
                // lowering the edge weight.
                {{GetKnownLanguageTag("en-001"), GetKnownLanguageTag("en-GB")},
                 0.8},
                {{GetKnownLanguageTag("en-001"), GetKnownLanguageTag("en-US")},
                 0.9},
                // For english (en) we favor "en-US" matches by lowering the
                // "en" -> "en-US" edge weight.
                {{GetKnownLanguageTag("en"), GetKnownLanguageTag("en-US")},
                 0.8},
                {{GetKnownLanguageTag("en"), GetKnownLanguageTag("en-GB")},
                 0.9},
                {{GetKnownLanguageTag("es"), GetKnownLanguageTag("es-419")},
                 0.8},
                {{GetKnownLanguageTag("pt"), GetKnownLanguageTag("pt-BR")},
                 0.8},
                {{GetKnownLanguageTag("zh"), GetKnownLanguageTag("zh-CN")},
                 0.8},
                {{GetKnownLanguageTag("zh-Hant"), GetKnownLanguageTag("zh-TW")},
                 0.8}};
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
  LanguageTagPreferenceGraph(const icu4x::LocaleFallbacker* icu_fallbacker,
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
        AddEdge(fallback_tag, previous);
        previous = fallback_tag;
      }

      // Supported locales are their own best match with 0 distance.
      SetDistance(supported_tag, supported_tag, 0);
    }

    // Special edges for Liberian and Philippino English to favor en-US, the
    // rest should default to en-GB.
    AddEdge(GetKnownLanguageTag("en-PH"), GetKnownLanguageTag("en-US"));
    AddEdge(GetKnownLanguageTag("en-LR"), GetKnownLanguageTag("en-US"));
    // Special case for "en-CA" which from ICU data will default to "en-US" but
    // Chorme i18n code always assumes it should match "en-GB".
    AddEdge(GetKnownLanguageTag("en-CA"), GetKnownLanguageTag("en-GB"));
    // This edge does not exist from the fallback algorithm as
    // fallback("en-GB") = ["en-001", "en"]
    // We need to add it to get "en" to match "en-GB" when "en-US" is not
    // present.
    AddEdge(GetKnownLanguageTag("en"), GetKnownLanguageTag("en-GB"));
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
  void AddEdge(LanguageTag source, LanguageTag target) {
    edges_[source].push_back(target);
    distance_.try_emplace(source, std::numeric_limits<float>::max());
  }

  void SetDistance(LanguageTag node, LanguageTag closest_node, float distance) {
    distance_.insert_or_assign(node, distance);
    closest_supported_tag_.insert_or_assign(node, closest_node);
  }

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
    SetDistance(current, best_result.closest_supported, best_result.distance);
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
  std::unique_ptr<icu4x::LocaleFallbacker> fallbacker =
      icu4x::LocaleFallbacker::create();
  LanguageTagPreferenceGraph graph(fallbacker.get(), supported_tags);

  return LanguageTagMatcher(std::move(graph).ComputeClosestSupportedTag(),
                            std::move(fallbacker));
}

bool LanguageTagMatcher::HasExactMatch(
    const LanguageTag& preferred_locale) const {
  auto it = closest_supported_tag_.find(preferred_locale);
  if (it == closest_supported_tag_.end()) {
    return false;
  }
  return it->second == preferred_locale;
}

std::optional<LanguageTag> LanguageTagMatcher::Match(
    const LanguageTag& preferred_locale) const {
  // Step 1: Check if the preferred locale is linked to a supported node.
  auto it = closest_supported_tag_.find(preferred_locale);
  if (it != closest_supported_tag_.end()) {
    return it->second;
  }
  // Step 2: Traverse the fallback chain to look for a supported locale. The
  // first supported locale found is returned.
  for (const LanguageTag& fallback :
       GetFallbackLocales(icu_fallbacker_.get(), preferred_locale)) {
    auto it_fallback = closest_supported_tag_.find(fallback);
    if (it_fallback != closest_supported_tag_.end()) {
      return it_fallback->second;
    }
  }

  return std::nullopt;
}

LanguageTagMatcher::LanguageTagMatcher(
    base::flat_map<LanguageTag, LanguageTag> closest_supported_tag,
    std::unique_ptr<icu4x::LocaleFallbacker> icu_fallbacker)
    : closest_supported_tag_(std::move(closest_supported_tag)),
      icu_fallbacker_(std::move(icu_fallbacker)) {}

LanguageTagMatcher::LanguageTagMatcher(LanguageTagMatcher&& other) noexcept =
    default;

LanguageTagMatcher& LanguageTagMatcher::operator=(
    LanguageTagMatcher&& other) noexcept = default;

LanguageTagMatcher::~LanguageTagMatcher() = default;

LanguageTagMatcherWithDefault::LanguageTagMatcherWithDefault(
    LanguageTag default_tag,
    LanguageTagMatcher matcher)
    : default_tag_(std::move(default_tag)), matcher_(std::move(matcher)) {}

std::optional<LanguageTag> LanguageTagMatcherWithDefault::Match(
    const LanguageTag& preferred_tag) const {
  return matcher_.Match(preferred_tag);
}

bool LanguageTagMatcherWithDefault::HasExactMatch(
    const LanguageTag& preferred_tag) const {
  return matcher_.HasExactMatch(preferred_tag);
}

LanguageTag LanguageTagMatcherWithDefault::MatchOrDefault(
    const LanguageTag& preferred_tag) const {
  return matcher_.Match(preferred_tag).value_or(default_tag_);
}

// static
LanguageTagMatcherWithDefault LanguageTagMatcherWithDefault::Create(
    LanguageTag default_tag,
    base::span<const LanguageTag> supported_tags) {
  return LanguageTagMatcherWithDefault(
      std::move(default_tag), LanguageTagMatcher::Create(supported_tags));
}

}  // namespace base::i18n
