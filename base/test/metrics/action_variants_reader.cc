// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/action_variants_reader.h"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libxml/chromium/xml_reader.h"

namespace base::test {

namespace {

// Parses a <variants> ... </variants> block from the XML file.
// Variants name passed as logging context in case of failure.
// The reader must be positioned on the <variants> node.
// Returns nullopt if parsing fails or the block is empty.
std::optional<ActionVariantsEntryMap> ParseVariants(
    std::string_view logging_context,
    XmlReader& reader,
    std::string_view separator) {
  if (reader.IsEmptyElement()) {
    return std::nullopt;
  }

  ActionVariantsEntryMap variants;
  const int parent_depth = reader.Depth();
  bool success = true;

  // The reader is on the <variants> node. We need to read until we find the
  // closing </variants> tag.
  while (reader.Read() && reader.Depth() > parent_depth) {
    if (!reader.IsElement()) {
      continue;
    }
    if (reader.NodeName() != "variant") {
      ADD_FAILURE() << "Unexpected node in variants block " << logging_context
                    << ": " << reader.NodeName();
      // Do not return yet, identify all bad variants.
      success = false;
      continue;
    }
    std::string name;
    std::string summary;
    if (!reader.NodeAttribute("name", &name) ||
        !reader.NodeAttribute("summary", &summary)) {
      ADD_FAILURE() << "Variant in " << logging_context
                    << " is missing name or summary.";
      // Do not return yet, identify all bad variants.
      success = false;
      continue;
    }
    // Do not count the base variant with empty name, such that in the example
    // below we count 2 variants, not 3:
    //   <variants name="ChromeOS_Settings_Languages_Type">
    //    <variant name="" summary="aggregated"/>
    //    <variant name=".AddInputMethod" summary="Users tapped 'Add method'"/>
    //    <variant name=".AddLanguages" summary="Users tapped 'Add languages'"/>
    //   </variants>
    if (name.empty()) {
      continue;
    }

    // Variants in the xml file start with the separator. If a symbol was
    // passed in place of separator argument, and the variant name starts
    // with that symbol, omit it.
    if (!separator.empty() && name.starts_with(separator)) {
      name = name.substr(1);
    }
    variants[name] = summary;
  }

  if (!success || variants.empty()) {
    return std::nullopt;
  }

  return variants;
}

// Parses an <action> node to find the variants for `affected_action`.
// Returns true if the action was found, in which case `result` will be
// populated and the calling loop should terminate.
bool ParseActionNode(
    XmlReader& reader,
    std::string_view affected_action,
    const std::map<std::string, ActionVariantsEntryMap>& global_variants,
    std::vector<ActionVariantsEntryMap>& result,
    std::string_view separator) {
  std::string name;
  if (!reader.NodeAttribute("name", &name)) {
    return false;
  }

  // In case of patterned action with {variant}, identify the base action name.
  // Locate the first bracket and return the preceding substring.
  std::string base_name = name;
  const size_t brace_pos = name.find('{');
  if (brace_pos != std::string::npos) {
    base_name = name.substr(0, brace_pos);
  }

  if (base_name != affected_action) {
    return false;
  }
  if (reader.IsEmptyElement()) {
    return false;
  }
  // Found the action. Now parse its tokens.
  const int parent_depth = reader.Depth();
  while (reader.Read() && reader.Depth() > parent_depth) {
    if (!reader.IsElement() || reader.NodeName() != "token") {
      continue;
    }

    std::string variants_name;
    if (reader.NodeAttribute("variants", &variants_name)) {
      // Out-of-line variants.
      auto it = global_variants.find(variants_name);
      if (it == global_variants.end()) {
        ADD_FAILURE() << "Variants block not found: " << variants_name;
        return false;
      }
      result.push_back(it->second);
    } else {
      auto variants = ParseVariants("inline", reader, separator);
      if (variants) {
        result.push_back(std::move(*variants));
      }
    }
  }
  // We found the action and processed it, so we can stop.
  return true;
}

std::vector<ActionVariantsEntryMap> ReadActionVariantsForActionImpl(
    XmlReader& reader,
    std::string_view affected_action,
    std::string_view separator) {
  std::vector<ActionVariantsEntryMap> result;
  std::map<std::string, ActionVariantsEntryMap> global_variants;

  // This is a manual depth-first traversal of the XML tree. We read each node
  // and only process the ones we care about. This is much safer than using
  // SkipToElement() and prevents the infinite loops that were causing timeouts.
  while (reader.Read()) {
    if (!reader.IsElement()) {
      continue;
    }

    const std::string node_name = reader.NodeName();

    // The <variants> blocks are expected to appear before any
    // <action> blocks that may use them.
    if (node_name == "variants") {
      std::string variants_name;
      if (reader.NodeAttribute("name", &variants_name)) {
        auto variants = ParseVariants(variants_name, reader, separator);
        if (variants) {
          global_variants[variants_name] = std::move(*variants);
        }
      }
    } else if (node_name == "action") {
      if (ParseActionNode(reader, affected_action, global_variants, result,
                          separator)) {
        // We found the action and processed it, so we can stop.
        return result;
      }
    }
  }

  return result;
}

}  // namespace

std::vector<ActionVariantsEntryMap> ReadActionVariantsForAction(
    std::string_view action_name,
    std::string_view separator) {
  base::FilePath actions_xml_path;
  // This path is from //chrome/test/data/
  CHECK(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &actions_xml_path));
  actions_xml_path = actions_xml_path.AppendASCII("tools")
                         .AppendASCII("metrics")
                         .AppendASCII("actions")
                         .AppendASCII("actions.xml");
  std::string xml_string;
  if (!base::ReadFileToString(actions_xml_path, &xml_string)) {
    ADD_FAILURE() << "Could not read " << actions_xml_path;
    return {};
  }
  XmlReader reader;
  if (!reader.Load(xml_string)) {
    ADD_FAILURE() << "Failed to load XML from string.";
    return {};
  }
  return ReadActionVariantsForActionImpl(reader, action_name, separator);
}

std::vector<ActionVariantsEntryMap> ReadActionVariantsForActionFromXmlString(
    std::string_view xml_string,
    std::string_view action_name,
    std::string_view separator) {
  XmlReader reader;
  CHECK(reader.Load(xml_string));
  return ReadActionVariantsForActionImpl(reader, action_name, separator);
}

}  // namespace base::test
