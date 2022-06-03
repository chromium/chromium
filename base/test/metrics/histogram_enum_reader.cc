// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_enum_reader.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/libxml/chromium/xml_reader.h"

namespace base {
namespace {

// This is a helper function to the ReadEnumFromHistogramsXml().
// Extracts single enum (with integer values) from histograms.xml.
// Expects |reader| to point at given enum.
// Returns map { value => label } on success, and nullopt on failure.
absl::optional<HistogramEnumEntryMap> ParseEnumFromHistogramsXml(
    const std::string& enum_name,
    XmlReader* reader) {
  int entries_index = -1;

  HistogramEnumEntryMap result;
  bool success = true;

  while (true) {
    const std::string node_name = reader->NodeName();
    if (node_name == "enum" && reader->IsClosingElement())
      break;

    if (node_name == "int") {
      ++entries_index;
      std::string value_str;
      std::string label;
      const bool has_value = reader->NodeAttribute("value", &value_str);
      const bool has_label = reader->NodeAttribute("label", &label);
      if (!has_value) {
        ADD_FAILURE() << "Bad " << enum_name << " enum entry (at index "
                      << entries_index << ", label='" << label
                      << "'): No 'value' attribute.";
        success = false;
      }
      if (!has_label) {
        ADD_FAILURE() << "Bad " << enum_name << " enum entry (at index "
                      << entries_index << ", value_str='" << value_str
                      << "'): No 'label' attribute.";
        success = false;
      }

      HistogramBase::Sample value;
      if (has_value && !StringToInt(value_str, &value)) {
        ADD_FAILURE() << "Bad " << enum_name << " enum entry (at index "
                      << entries_index << ", label='" << label
                      << "', value_str='" << value_str
                      << "'): 'value' attribute is not integer.";
        success = false;
      }
      if (result.count(value)) {
        ADD_FAILURE() << "Bad " << enum_name << " enum entry (at index "
                      << entries_index << ", label='" << label
                      << "', value_str='" << value_str
                      << "'): duplicate value '" << value_str
                      << "' found in enum. The previous one has label='"
                      << result[value] << "'.";
        success = false;
      }
      if (success)
        result[value] = label;
    }
    // All enum entries are on the same level, so it is enough to iterate
    // until possible.
    reader->Next();
  }
  if (success)
    return result;
  return absl::nullopt;
}

}  // namespace

absl::optional<HistogramEnumEntryMap> ReadEnumFromEnumsXml(
    const std::string& enum_name) {
  FilePath src_root;
  if (!PathService::Get(DIR_SOURCE_ROOT, &src_root)) {
    ADD_FAILURE() << "Failed to get src root.";
    return absl::nullopt;
  }

  base::FilePath enums_xml = src_root.AppendASCII("tools")
                                 .AppendASCII("metrics")
                                 .AppendASCII("histograms")
                                 .AppendASCII("enums.xml");
  if (!PathExists(enums_xml)) {
    ADD_FAILURE() << "enums.xml file does not exist.";
    return absl::nullopt;
  }

  XmlReader enums_xml_reader;
  if (!enums_xml_reader.LoadFile(enums_xml.MaybeAsASCII())) {
    ADD_FAILURE() << "Failed to load enums.xml";
    return absl::nullopt;
  }

  absl::optional<HistogramEnumEntryMap> result;

  // Implement simple depth first search.
  while (true) {
    const std::string node_name = enums_xml_reader.NodeName();
    if (node_name == "enum") {
      std::string name;
      if (enums_xml_reader.NodeAttribute("name", &name) && name == enum_name) {
        if (result.has_value()) {
          ADD_FAILURE() << "Duplicate enum '" << enum_name
                        << "' found in enums.xml";
          return absl::nullopt;
        }

        const bool got_into_enum = enums_xml_reader.Read();
        if (!got_into_enum) {
          ADD_FAILURE() << "Bad enum '" << enum_name
                        << "' (looks empty) found in enums.xml.";
          return absl::nullopt;
        }

        result = ParseEnumFromHistogramsXml(enum_name, &enums_xml_reader);
        if (!result.has_value()) {
          ADD_FAILURE() << "Bad enum '" << enum_name
                        << "' found in histograms.xml (format error).";
          return absl::nullopt;
        }
      }
    }
    // Go deeper if possible (stops at the closing tag of the deepest node).
    if (enums_xml_reader.Read())
      continue;

    // Try next node on the same level (skips closing tag).
    if (enums_xml_reader.Next())
      continue;

    // Go up until next node on the same level exists.
    while (enums_xml_reader.Depth() && !enums_xml_reader.SkipToElement()) {
    }

    // Reached top. histograms.xml consists of the single top level node
    // 'histogram-configuration', so this is the end.
    if (!enums_xml_reader.Depth())
      break;
  }
  return result;
}

}  // namespace base
