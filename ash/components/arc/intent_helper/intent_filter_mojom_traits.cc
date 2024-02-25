// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/intent_helper/intent_filter_mojom_traits.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_util.h"

namespace mojo {

bool StructTraits<arc::mojom::IntentFilterDataView, arc::IntentFilter>::Read(
    arc::mojom::IntentFilterDataView data,
    arc::IntentFilter* out) {
  std::vector<arc::IntentFilter::AuthorityEntry> authorities;
  if (!data.ReadDataAuthorities(&authorities))
    return false;

  std::vector<arc::IntentFilter::PatternMatcher> paths;
  if (!data.ReadDataPaths(&paths))
    return false;

  std::optional<std::string> package_name;
  if (!data.ReadPackageName(&package_name))
    return false;

  std::vector<std::string> schemes;
  if (!data.ReadDataSchemes(&schemes))
    return false;

  std::vector<std::string> actions;
  if (!data.ReadActions(&actions))
    return false;

  std::vector<std::string> mime_types;
  if (!data.ReadMimeTypes(&mime_types))
    return false;

  std::optional<std::string> activity_name;
  if (!data.ReadActivityName(&activity_name))
    return false;

  std::optional<std::string> activity_label;
  if (!data.ReadActivityLabel(&activity_label))
    return false;

  *out = arc::IntentFilter(std::move(package_name).value_or(std::string()),
                           std::move(activity_name).value_or(std::string()),
                           std::move(activity_label).value_or(std::string()),
                           std::move(actions), std::move(authorities),
                           std::move(paths), std::move(schemes),
                           std::move(mime_types));
  return true;
}

bool StructTraits<arc::mojom::AuthorityEntryDataView,
                  arc::IntentFilter::AuthorityEntry>::
    Read(arc::mojom::AuthorityEntryDataView data,
         arc::IntentFilter::AuthorityEntry* out) {
  std::string host;
  if (!data.ReadHost(&host))
    return false;

  *out = arc::IntentFilter::AuthorityEntry(std::move(host), data.port());
  return true;
}

bool StructTraits<arc::mojom::PatternMatcherDataView,
                  arc::IntentFilter::PatternMatcher>::
    Read(arc::mojom::PatternMatcherDataView data,
         arc::IntentFilter::PatternMatcher* out) {
  std::string pattern;
  if (!data.ReadPattern(&pattern))
    return false;

  arc::mojom::PatternType type;
  if (!data.ReadType(&type))
    return false;

  *out = arc::IntentFilter::PatternMatcher(std::move(pattern), type);
  return true;
}

}  // namespace mojo
