// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/omnibox/suggestion_parser.h"

#include <string_view>

#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/extensions/api/omnibox.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/safe_xml_parser.h"
#include "services/data_decoder/public/mojom/xml_parser.mojom.h"

namespace extensions {

namespace omnibox = api::omnibox;

namespace {

// A helper function to DCHECK() that retrieving the tag for a given XML node
// succeeds and return the tag.
std::string CheckedGetElementTag(const base::Value& node) {
  std::string tag;
  bool got_tag = data_decoder::GetXmlElementTagName(node, &tag);
  DCHECK(got_tag);
  return tag;
}

// Recursively walks an XML node, generating `result` as it goes along.
void WalkNode(const base::Value& node, DescriptionAndStyles* result) {
  const base::Value::List* children = data_decoder::GetXmlElementChildren(node);
  if (!children)
    return;

  for (const base::Value& child : *children) {
    // Append text nodes to our description.
    if (data_decoder::IsXmlElementOfType(
            child, data_decoder::mojom::XmlParser::kTextNodeType)) {
      DCHECK(child.is_dict());
      const std::string* text =
          child.GetDict().FindString(data_decoder::mojom::XmlParser::kTextKey);
      DCHECK(text);
      std::u16string sanitized_text = base::UTF8ToUTF16(*text);
      // Note: We unfortunately can't just use
      // `AutocompleteMatch::SanitizeString()` directly here, because it
      // unconditionally trims leading whitespace, which we need to preserve
      // for any non-first styles.
      // TODO(devlin): Add a toggle to AutocompleteMatch::SanitizeString() for
      // that?
      base::RemoveChars(sanitized_text, AutocompleteMatch::kInvalidChars,
                        &sanitized_text);
      if (result->description.empty()) {
        base::TrimWhitespace(sanitized_text, base::TRIM_LEADING,
                             &sanitized_text);
      }
      result->description += sanitized_text;
      continue;
    }

    if (!data_decoder::IsXmlElementOfType(
            child, data_decoder::mojom::XmlParser::kElementType)) {
      // Unsupported node type. Even so, we walk all children in the node for
      // forward compatibility.
      WalkNode(child, result);
      continue;
    }
    std::string tag = CheckedGetElementTag(child);
    omnibox::DescriptionStyleType style_type =
        omnibox::ParseDescriptionStyleType(tag);
    if (style_type == omnibox::DescriptionStyleType::kNone) {
      // Unsupported style type. Even so, we walk all children in the node for
      // forward compatibility.
      WalkNode(child, result);
      continue;
    }

    int current_index = result->styles.size();
    result->styles.emplace_back();
    // Note: We don't cache a reference to the newly-created entry because
    // the recursive WalkNode() call might change the vector, which can
    // invalidate references.
    result->styles[current_index].type = style_type;
    int offset = result->description.length();
    result->styles[current_index].offset = offset;
    WalkNode(child, result);
    result->styles[current_index].length =
        result->description.length() - offset;
  }
}

// Populates `entries` with individual suggestions contained within
// `root-node`. Used when we construct an XML document with multiple suggestions
// for parsing.
// Returns true on success. `entries_out` may be modified even if population
// fails.
bool PopulateEntriesFromNode(const base::Value& root_node,
                             std::vector<const base::Value*>* entries_out) {
  if (!data_decoder::IsXmlElementOfType(
          root_node, data_decoder::mojom::XmlParser::kElementType)) {
    return false;
  }

  if (CheckedGetElementTag(root_node) != "fragment")
    return false;

  const base::Value::List* children =
      data_decoder::GetXmlElementChildren(root_node);
  if (!children)
    return false;

  entries_out->reserve(children->size());
  for (const base::Value& child : *children) {
    if (!data_decoder::IsXmlElementOfType(
            child, data_decoder::mojom::XmlParser::kElementType)) {
      return false;
    }
    if (CheckedGetElementTag(child) != "internal-suggestion")
      return false;
    entries_out->push_back(&child);
  }

  return true;
}

// Constructs the DescriptionAndStylesResult from the parsed XML value, if
// present.
void ConstructResultFromValue(
    DescriptionAndStylesCallback callback,
    bool has_multiple_entries,
    data_decoder::DataDecoder::ValueOrError value_or_error) {
  DescriptionAndStylesResult result;

  // A helper function to set an error and run the callback.
  auto run_callback_with_error = [&result, &callback](std::string error) {
    DCHECK_EQ(0u, result.descriptions_and_styles.size());
    result.error = std::move(error);
    std::move(callback).Run(std::move(result));
  };

  if (!value_or_error.has_value()) {
    run_callback_with_error(std::move(value_or_error.error()));
    return;
  }
  const base::Value root_node = std::move(*value_or_error);

  // From this point on, we hope that everything is valid (e.g., that we don't
  // get non-dictionary values or unexpected top-level types. But, if we did,
  // emit a generic error.
  constexpr char kGenericError[] = "Invalid XML";

  if (!root_node.is_dict()) {
    run_callback_with_error(kGenericError);
    return;
  }

  std::vector<const base::Value*> entries;
  if (has_multiple_entries) {
    if (!PopulateEntriesFromNode(root_node, &entries)) {
      run_callback_with_error(kGenericError);
      return;
    }
  } else {
    entries.push_back(&root_node);
  }

  result.descriptions_and_styles.reserve(entries.size());
  for (const base::Value* entry : entries) {
    DescriptionAndStyles parsed;
    WalkNode(*entry, &parsed);
    result.descriptions_and_styles.push_back(std::move(parsed));
  }

  std::move(callback).Run(std::move(result));
}

// A helper method for ParseDescriptionsAndStyles(). `contains_multiple_entries`
// indicates whether `xml_input` contains a single suggestion or multiple
// suggestions that have been wrapped in individual XML elements.
void ParseDescriptionAndStylesImpl(std::string_view xml_input,
                                   bool contains_multiple_entries,
                                   DescriptionAndStylesCallback callback) {
  std::string wrapped_xml =
      base::StringPrintf("<fragment>%s</fragment>", xml_input.data());
  data_decoder::DataDecoder::ParseXmlIsolated(
      wrapped_xml,
      data_decoder::mojom::XmlParser::WhitespaceBehavior::kPreserveSignificant,
      base::BindOnce(&ConstructResultFromValue, std::move(callback),
                     contains_multiple_entries));
}

}  // namespace

DescriptionAndStyles::DescriptionAndStyles() = default;
DescriptionAndStyles::DescriptionAndStyles(DescriptionAndStyles&&) = default;
DescriptionAndStyles& DescriptionAndStyles::operator=(DescriptionAndStyles&&) =
    default;
DescriptionAndStyles::~DescriptionAndStyles() = default;

DescriptionAndStylesResult::DescriptionAndStylesResult() = default;
DescriptionAndStylesResult::DescriptionAndStylesResult(
    DescriptionAndStylesResult&&) = default;
DescriptionAndStylesResult& DescriptionAndStylesResult::operator=(
    DescriptionAndStylesResult&&) = default;
DescriptionAndStylesResult::~DescriptionAndStylesResult() = default;

void ParseDescriptionAndStyles(std::string_view str,
                               DescriptionAndStylesCallback callback) {
  constexpr bool kContainsMultipleEntries = false;
  ParseDescriptionAndStylesImpl(str, kContainsMultipleEntries,
                                std::move(callback));
}

void ParseDescriptionsAndStyles(const std::vector<std::string_view>& strs,
                                DescriptionAndStylesCallback callback) {
  // When passed multiple suggestions, we synthesize them into a single XML
  // document. This allows us to parse all of them with a single call to the
  // XML parser. We then separate them again when constructing the result.
  // For instance, given the input:
  // { "suggestion one", "suggestion two" }
  // We construct the XML (once wrapped):
  // <fragment>
  //   <internal-suggestion>suggestion one</internal-suggestion>
  //   <internal-suggestion>suggestion two</internal-suggestion>
  // </fragment>
  // It's fine for the input to have unexpected XML tags; it just results in
  // either invalid XML (if they're unbalanced) or unexpected children, which
  // are safely ignored in our result handling.
  std::string xml_input;
  for (const auto& str : strs) {
    xml_input += base::StringPrintf(
        "<internal-suggestion>%s</internal-suggestion>", str.data());
  }
  constexpr bool kContainsMultipleEntries = true;
  ParseDescriptionAndStylesImpl(xml_input, kContainsMultipleEntries,
                                std::move(callback));
}

}  // namespace extensions
