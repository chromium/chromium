// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_variants_reader.h"

#include <map>
#include <optional>
#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libxml/chromium/xml_reader.h"

namespace base {

namespace {

// Extracts single variants block from a histograms.xml.
//
// Expects |reader| to point at the given <variants> element with the name
// `variants_name`.
//
// Returns map { name => summary } on success, and nullopt on failure.
std::optional<HistogramVariantsEntryMap> ParseVariantsFromHistogramsXml(
    const std::string& variants_name,
    XmlReader& reader) {
  HistogramVariantsEntryMap result;
  bool success = true;

  while (true) {
    // Because reader initially points to the start of the <variants> element,
    // and because <variants> elements are not nested, when the closing tag is
    // reached, parsing is complete.
    const std::string node_name = reader.NodeName();
    if (node_name == "variants" && reader.IsClosingElement()) {
      break;
    }

    if (node_name == "variant") {
      std::string name;
      std::string summary;
      const bool has_name = reader.NodeAttribute("name", &name);
      const bool has_summary = reader.NodeAttribute("summary", &summary);

      if (!has_name) {
        ADD_FAILURE() << "Bad " << variants_name << " variant entry, summary='"
                      << summary << "'): No 'name' attribute.";
        success = false;
      }

      if (!has_summary) {
        ADD_FAILURE() << "Bad " << variants_name << " variant entry, name='"
                      << name << "'): No 'summary' attribute.";
        success = false;
      }

      // Don't check summary here because we want to check for duplicate names,
      // and if there was a missing summary the function has already failed.
      if (has_name) {
        const auto insert_result = result.emplace(name, summary);
        if (!insert_result.second) {
          ADD_FAILURE() << "Duplicate entry in " << variants_name
                        << " variant entry, name='" << name << ')';
          success = false;
        }
      }
    }

    // All variant entries are on the same level, so advance to the next
    // sibling.
    reader.Next();
  }

  return success ? std::make_optional(result) : std::nullopt;
}

}  // namespace

std::optional<HistogramVariantsEntryMap> ReadVariantsFromHistogramsXml(
    const std::string& variants_name,
    const std::string& subdirectory,
    bool from_metadata) {
  base::FilePath src_root;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root)) {
    ADD_FAILURE() << "Failed to get src root.";
    return std::nullopt;
  }

  base::FilePath path =
      src_root.AppendASCII("tools").AppendASCII("metrics").AppendASCII(
          "histograms");
  if (from_metadata) {
    path = path.AppendASCII("metadata");
  }
  if (!subdirectory.empty()) {
    path = path.AppendASCII(subdirectory);
  }
  path = path.AppendASCII("histograms.xml");

  if (!base::PathExists(path)) {
    ADD_FAILURE() << "File does not exist: " << path;
    return std::nullopt;
  }

  XmlReader reader;
  if (!reader.LoadFile(path.MaybeAsASCII())) {
    ADD_FAILURE() << "Failed to load " << path;
    return std::nullopt;
  }

  std::optional<HistogramVariantsEntryMap> result;

  // Implement simple depth first search.
  while (true) {
    const std::string node_name = reader.NodeName();
    if (node_name == "variants") {
      std::string name;
      if (reader.NodeAttribute("name", &name) && name == variants_name) {
        if (result.has_value()) {
          ADD_FAILURE() << "Duplicate variant '" << variants_name
                        << "' found in " << path;
          return std::nullopt;
        }

        const bool got_into_variant = reader.Read();
        if (!got_into_variant) {
          ADD_FAILURE() << "Bad variant '" << variants_name
                        << "' (looks empty) found in " << path;
          return std::nullopt;
        }

        result = ParseVariantsFromHistogramsXml(variants_name, reader);
        if (!result.has_value()) {
          ADD_FAILURE() << "Bad variant '" << variants_name << "' found in "
                        << path << " (format error).";
          return std::nullopt;
        }
      }
    }

    // Go deeper if possible (stops at the closing tag of the deepest node).
    if (reader.Read()) {
      continue;
    }

    // Try next node on the same level (skips closing tag).
    if (reader.Next()) {
      continue;
    }

    // Go up until next node on the same level exists.
    while (reader.Depth() && !reader.SkipToElement()) {
    }

    // Reached top. histograms.xml consists of the single top level node
    // 'histogram-configuration', so this is the end.
    if (!reader.Depth()) {
      break;
    }
  }

  return result;
}

}  // namespace base
