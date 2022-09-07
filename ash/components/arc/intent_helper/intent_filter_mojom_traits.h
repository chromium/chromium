// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_INTENT_HELPER_INTENT_FILTER_MOJOM_TRAITS_H_
#define ASH_COMPONENTS_ARC_INTENT_HELPER_INTENT_FILTER_MOJOM_TRAITS_H_

#include <string>
#include <vector>

#include "ash/components/arc/mojom/intent_helper.mojom-shared.h"
#include "base/containers/span.h"
#include "components/arc/intent_helper/intent_filter.h"

namespace mojo {

template <>
struct StructTraits<arc::mojom::IntentFilterDataView, arc::IntentFilter> {
  static const std::vector<std::string>& actions(const arc::IntentFilter& r) {
    return r.actions();
  }
  static const base::span<std::string> categories(const arc::IntentFilter& r) {
    // Returns an empty array.
    return base::span<std::string>();
  }
  static const std::vector<std::string>& data_schemes(
      const arc::IntentFilter& r) {
    return r.schemes();
  }
  static const std::vector<arc::IntentFilter::AuthorityEntry>& data_authorities(
      const arc::IntentFilter& r) {
    return r.authorities();
  }
  static const std::vector<arc::IntentFilter::PatternMatcher>& data_paths(
      const arc::IntentFilter& r) {
    return r.paths();
  }
  static const base::span<arc::IntentFilter::PatternMatcher>
  deprecated_data_scheme_specific_parts(const arc::IntentFilter& r) {
    // Returns an empty array.
    return base::span<arc::IntentFilter::PatternMatcher>();
  }

  static const std::string& package_name(const arc::IntentFilter& r) {
    return r.package_name();
  }

  static const std::vector<std::string>& mime_types(
      const arc::IntentFilter& r) {
    return r.mime_types();
  }

  static const std::string& activity_name(const arc::IntentFilter& r) {
    return r.activity_name();
  }

  static const std::string& activity_label(const arc::IntentFilter& r) {
    return r.activity_label();
  }

  static bool Read(arc::mojom::IntentFilterDataView data,
                   arc::IntentFilter* out);
};

template <>
struct StructTraits<arc::mojom::AuthorityEntryDataView,
                    arc::IntentFilter::AuthorityEntry> {
  static const std::string& host(const arc::IntentFilter::AuthorityEntry& r) {
    return r.host();
  }
  static int32_t port(const arc::IntentFilter::AuthorityEntry& r) {
    return r.port();
  }

  static bool Read(arc::mojom::AuthorityEntryDataView data,
                   arc::IntentFilter::AuthorityEntry* out);
};

template <>
struct StructTraits<arc::mojom::PatternMatcherDataView,
                    arc::IntentFilter::PatternMatcher> {
  static const std::string& pattern(
      const arc::IntentFilter::PatternMatcher& r) {
    return r.pattern();
  }
  static arc::mojom::PatternType type(
      const arc::IntentFilter::PatternMatcher& r) {
    return r.match_type();
  }

  static bool Read(arc::mojom::PatternMatcherDataView data,
                   arc::IntentFilter::PatternMatcher* out);
};

}  // namespace mojo

#endif  // ASH_COMPONENTS_ARC_INTENT_HELPER_INTENT_FILTER_MOJOM_TRAITS_H_
