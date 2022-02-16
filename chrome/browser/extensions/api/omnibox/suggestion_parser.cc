// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/omnibox/suggestion_parser.h"

#include "base/callback.h"
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

// Recursively walks an XML node, generating `result` as it goes along.
void WalkNode(const base::Value& node, DescriptionAndStyles* result) {
  const base::Value* children = data_decoder::GetXmlElementChildren(node);
  if (!children)
    return;

  DCHECK(children->is_list());
  for (const base::Value& child : children->GetListDeprecated()) {
    // Append text nodes to our description.
    if (data_decoder::IsXmlElementOfType(
            child, data_decoder::mojom::XmlParser::kTextNodeType)) {
      const std::string* text =
          child.FindStringPath(data_decoder::mojom::XmlParser::kTextKey);
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
    std::string tag;
    bool got_tag = data_decoder::GetXmlElementTagName(child, &tag);
    DCHECK(got_tag);
    omnibox::DescriptionStyleType style_type =
        omnibox::ParseDescriptionStyleType(tag);
    if (style_type == omnibox::DESCRIPTION_STYLE_TYPE_NONE) {
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
        std::make_unique<int>(result->description.length() - offset);
  }
}

// Constructs the DescriptionAndStyles result from the parsed XML value, if
// present.
void ConstructResultFromValue(
    DescriptionAndStylesCallback callback,
    data_decoder::DataDecoder::ValueOrError value_or_error) {
  if (value_or_error.error) {
    std::move(callback).Run(nullptr);
    return;
  }

  DCHECK(value_or_error.value);
  if (!value_or_error.value->is_dict()) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto result = std::make_unique<DescriptionAndStyles>();
  WalkNode(*value_or_error.value, result.get());
  std::move(callback).Run(std::move(result));
}

}  // namespace

DescriptionAndStyles::DescriptionAndStyles() = default;
DescriptionAndStyles::~DescriptionAndStyles() = default;

void ParseDescriptionAndStyles(base::StringPiece str,
                               DescriptionAndStylesCallback callback) {
  std::string wrapped_xml =
      base::StringPrintf("<fragment>%s</fragment>", str.data());
  data_decoder::DataDecoder::ParseXmlIsolated(
      wrapped_xml,
      data_decoder::mojom::XmlParser::WhitespaceBehavior::kPreserveSignificant,
      base::BindOnce(&ConstructResultFromValue, std::move(callback)));
}

}  // namespace extensions
