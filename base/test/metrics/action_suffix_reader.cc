// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/action_suffix_reader.h"

#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libxml/chromium/xml_reader.h"

namespace base {

namespace {

// Extracts suffixes from a histograms.xml if the suffixes apply to
// `affected_action`, otherwise null.
//
// Expects |reader| to point at the starting node of the suffixes block.
//
// Returns map { name => label } on success, and nullopt on failure.
std::optional<ActionSuffixEntryMap> ParseActionSuffixesFromActionsXml(
    const std::string& affected_action,
    XmlReader& reader) {
  ActionSuffixEntryMap result;
  std::vector<std::string> failures;
  bool action_found = false;

  while (true) {
    // Because reader initially points to the start of the <action-suffix>
    // element, and because <suffix> and <affected-action> elements are not
    // nested, when the closing tag is reached, parsing is complete.
    const std::string node_name = reader.NodeName();
    if (node_name == "action-suffix" && reader.IsClosingElement()) {
      break;
    }

    std::string name;

    // Affected actions can be anywhere in the XML block, so just check if the
    // one the caller cares about is present.
    if (node_name == "affected-action" && reader.NodeAttribute("name", &name) &&
        name == affected_action) {
      action_found = true;
    }

    // The other thing found in this block is the list of suffixes. Capture them
    // all, recording failures, and then later the list will be returned if the
    // action was found.
    if (node_name == "suffix") {
      std::string label;
      const bool has_name = reader.NodeAttribute("name", &name);
      const bool has_label = reader.NodeAttribute("label", &label);

      if (!has_name) {
        failures.emplace_back(StringPrintf(
            "Bad suffix entry with label \"%s\"; no 'name' attribute.",
            label.c_str()));
      }

      if (!has_label) {
        failures.emplace_back(StringPrintf(
            "Bad suffix entry with name \"%s\"; no 'label' attribute.",
            name.c_str()));
      }

      // Don't check label here because we want to check for duplicate names,
      // and if there was a missing label the function has already failed.
      if (has_name) {
        const auto insert_result = result.emplace(name, label);
        if (!insert_result.second) {
          failures.emplace_back(
              StringPrintf("Duplicate suffix name \"%s\"", name.c_str()));
        }
      }
    }

    // All variant entries are on the same level, so advance to the next
    // sibling.
    reader.Next();
  }

  if (!action_found) {
    return std::nullopt;
  }

  if (!failures.empty()) {
    for (const auto& failure : failures) {
      ADD_FAILURE() << failure;
    }
    return std::nullopt;
  }

  return result;
}

std::vector<ActionSuffixEntryMap> ReadActionSuffixesForActionImpl(
    XmlReader& reader,
    const std::string& affected_action) {
  std::vector<ActionSuffixEntryMap> result;

  // Implement simple depth first search.
  while (true) {
    const std::string node_name = reader.NodeName();
    if (node_name == "action-suffix") {
      // Try to step into the node.
      if (reader.Read()) {
        auto suffixes =
            ParseActionSuffixesFromActionsXml(affected_action, reader);
        if (suffixes) {
          result.emplace_back(std::move(*suffixes));
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

    // Reached top. actions.xml consists of the single top level node 'actions',
    // so this is the end.
    if (!reader.Depth()) {
      break;
    }
  }

  return result;
}

}  // namespace

// Hidden function that reads from `xml_string` instead of actions.xml.
// Used to unit test the internal logic.
std::vector<ActionSuffixEntryMap> ReadActionSuffixesForActionForTesting(
    const std::string& xml_string,
    const std::string& affected_action) {
  XmlReader reader;
  CHECK(reader.Load(xml_string));
  return ReadActionSuffixesForActionImpl(reader, affected_action);
}

std::vector<ActionSuffixEntryMap> ReadActionSuffixesForAction(
    const std::string& affected_action) {
  FilePath src_root;
  if (!PathService::Get(DIR_SRC_TEST_DATA_ROOT, &src_root)) {
    ADD_FAILURE() << "Failed to get src root.";
    return {};
  }

  const FilePath path = src_root.AppendASCII("tools")
                            .AppendASCII("metrics")
                            .AppendASCII("actions")
                            .AppendASCII("actions.xml");

  if (!PathExists(path)) {
    ADD_FAILURE() << "File does not exist: " << path;
    return {};
  }

  XmlReader reader;
  if (!reader.LoadFile(path.MaybeAsASCII())) {
    ADD_FAILURE() << "Failed to load " << path;
    return {};
  }

  return ReadActionSuffixesForActionImpl(reader, affected_action);
}

}  // namespace base
