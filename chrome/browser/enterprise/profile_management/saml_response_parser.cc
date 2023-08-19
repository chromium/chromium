// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/profile_management/saml_response_parser.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/containers/flat_map.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace profile_management {

namespace {
constexpr char kChildrenKey[] = "children";

// Finds the value of the attribute named SAMLResponse from `dict` by doing a
// depth-first search in `dict`.
// Returns a pointer to the value or nullptr if nothing was found.
const std::string* GetSamlResponseFromDict(const base::Value::Dict& dict) {
  const std::string* attribute_name =
      dict.FindStringByDottedPath("attributes.name");
  if (attribute_name && *attribute_name == "SAMLResponse") {
    return dict.FindStringByDottedPath("attributes.value");
  }
  const base::Value::List* children = dict.FindList(kChildrenKey);
  if (!children) {
    return nullptr;
  }
  for (const auto& child : *children) {
    if (!child.is_dict()) {
      continue;
    }
    const auto* child_saml_response = GetSamlResponseFromDict(child.GetDict());
    if (child_saml_response) {
      return child_saml_response;
    }
  }
  return nullptr;
}

// Finds the value of `attribute` from `dict` by doing a depth-first search in
// `dict`. First we find an attribute with "Name" == `attribute`, then we try
// to find a children of that attribute with the tag AttributeValue. That
// attribute itself has children, and we look for the value the children with a
// "text" key.
// Returns a pointer to the value or nullptr if nothing was found.
const std::string* GetAttributeValue(const base::Value::Dict& dict,
                                     const std::string& attribute) {
  const std::string* attribute_name =
      dict.FindStringByDottedPath("attributes.Name");
  if (attribute_name && *attribute_name == attribute) {
    const base::Value::List* attribute_children = dict.FindList(kChildrenKey);
    if (!attribute_children) {
      return nullptr;
    }
    for (const auto& attribute_child : *attribute_children) {
      const auto& attribute_child_dict = attribute_child.GetDict();
      const std::string* tag = attribute_child_dict.FindString("tag");
      if (!tag || *tag != "AttributeValue") {
        continue;
      }

      const base::Value::List* attribute_value_children =
          attribute_child_dict.FindList(kChildrenKey);
      if (!attribute_value_children) {
        continue;
      }
      for (const auto& attribute_value_item : *attribute_value_children) {
        auto* attribute_value_dict = attribute_value_item.GetIfDict();
        if (attribute_value_dict) {
          return attribute_value_dict->FindString("text");
        }
      }
    }
  }

  const base::Value::List* children = dict.FindList(kChildrenKey);
  if (!children) {
    return nullptr;
  }
  for (const auto& child : *children) {
    if (!child.is_dict()) {
      continue;
    }
    const std::string* value = GetAttributeValue(child.GetDict(), attribute);
    if (value) {
      return value;
    }
  }
  return nullptr;
}

}  // namespace

SAMLResponseParser::SAMLResponseParser(std::vector<std::string>&& attributes,
                                       const std::string& body,
                                       ResponseParserCallback callback)
    : attributes_(std::move(attributes)), callback_(std::move(callback)) {
  data_decoder::DataDecoder::ParseXmlIsolated(
      body,
      data_decoder::mojom::XmlParser::WhitespaceBehavior::kPreserveSignificant,
      base::BindOnce(&SAMLResponseParser::GetSamlResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

SAMLResponseParser::~SAMLResponseParser() = default;

void SAMLResponseParser::GetSamlResponse(
    data_decoder::DataDecoder::ValueOrError value_or_error) {
  if (!value_or_error.has_value() || !value_or_error.value().is_dict()) {
    std::move(callback_).Run(base::flat_map<std::string, std::string>());
    return;
  }

  const auto* saml_response =
      GetSamlResponseFromDict(value_or_error.value().GetDict());

  if (!saml_response) {
    std::move(callback_).Run(base::flat_map<std::string, std::string>());
    return;
  }

  std::string decoded_response_value;
  if (!base::Base64Decode(*saml_response, &decoded_response_value)) {
    std::move(callback_).Run(base::flat_map<std::string, std::string>());
    return;
  }

  data_decoder::DataDecoder::ParseXmlIsolated(
      decoded_response_value,
      data_decoder::mojom::XmlParser::WhitespaceBehavior::kPreserveSignificant,
      base::BindOnce(&SAMLResponseParser::GetAttributesFromSAMLResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SAMLResponseParser::GetAttributesFromSAMLResponse(
    data_decoder::DataDecoder::ValueOrError value_or_error) {
  base::flat_map<std::string, std::string> result;
  if (!value_or_error.has_value() || !value_or_error.value().is_dict()) {
    std::move(callback_).Run(std::move(result));
    return;
  }

  // Set standard attributes.
  auto* destination_url =
      value_or_error.value().GetDict().FindStringByDottedPath(
          "attributes.Destination");
  if (destination_url) {
    result[kDestinationUrl] = *destination_url;
  }

  // Set custom attributes.
  for (const auto& attribute : attributes_) {
    const auto* value =
        GetAttributeValue(value_or_error.value().GetDict(), attribute);
    if (value) {
      result[attribute] = *value;
    }
  }

  std::move(callback_).Run(std::move(result));
}

}  // namespace profile_management
