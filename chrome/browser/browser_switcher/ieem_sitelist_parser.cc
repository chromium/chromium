// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/ieem_sitelist_parser.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "services/data_decoder/public/cpp/safe_xml_parser.h"
#include "services/data_decoder/public/mojom/constants.mojom.h"
#include "services/data_decoder/public/mojom/xml_parser.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace browser_switcher {

namespace {

using namespace data_decoder;  // For |GetXmlElement...()| helper functions.

const char kInvalidRootElement[] = "Invalid XML root element";

const char kSchema1RulesElement[] = "rules";
const char kSchema1EmieElement[] = "emie";
const char kSchema1DocModeElement[] = "docMode";
const char kSchema1DomainElement[] = "domain";
const char kSchema1PathElement[] = "path";
const char kSchema1ExcludeAttribute[] = "exclude";
const char kSchema1DoNotTransitionAttribute[] = "doNotTransition";
const char kSchema1TrueValue[] = "true";

const char kSchema2SiteListElement[] = "site-list";
const char kSchema2SiteElement[] = "site";
const char kSchema2SiteUrlAttribute[] = "url";
const char kSchema2SiteOpenInElement[] = "open-in";

std::vector<const base::Value*> GetChildrenWithTag(const base::Value& node,
                                                   const std::string& tag) {
  std::vector<const base::Value*> children;
  GetAllXmlElementChildrenWithTag(node, tag, &children);
  return children;
}

// Data in a v.1 schema <domain> or <path> element.
struct Entry {
  // URL or path concerned.
  std::string text;
  // Whether to include or exclude the URL.
  bool exclude;
  // List affected by this rule (sitelist or greylist).
  std::vector<std::string>* list;
};

Entry ParseDomainOrPath(const base::Value& node, ParsedXml* result) {
  DCHECK(IsXmlElementNamed(node, kSchema1DomainElement) ||
         IsXmlElementNamed(node, kSchema1PathElement));

  Entry entry;

  std::string exclude_attrib =
      GetXmlElementAttribute(node, kSchema1ExcludeAttribute);
  entry.exclude = (exclude_attrib == kSchema1TrueValue);

  std::string do_not_transition_attrib =
      GetXmlElementAttribute(node, kSchema1DoNotTransitionAttribute);
  entry.list =
      ((do_not_transition_attrib == kSchema1TrueValue) ? &result->greylist
                                                       : &result->sitelist);

  GetXmlElementText(node, &entry.text);
  base::TrimWhitespaceASCII(entry.text, base::TRIM_ALL, &entry.text);

  return entry;
}

// Parses Enterprise Mode schema 1 files according to:
// https://technet.microsoft.com/itpro/internet-explorer/ie11-deploy-guide/enterprise-mode-schema-version-1-guidance
void ParseIeFileVersionOne(const base::Value& xml, ParsedXml* result) {
  DCHECK(IsXmlElementNamed(xml, kSchema1RulesElement));
  for (const base::Value& node : GetXmlElementChildren(xml)->GetList()) {
    // Skip over anything that is not a <emie> or <docMode> element.
    if (!IsXmlElementNamed(node, kSchema1EmieElement) &&
        !IsXmlElementNamed(node, kSchema1DocModeElement)) {
      continue;
    }
    // Loop over <domain> elements.
    for (const base::Value* domain_node :
         GetChildrenWithTag(node, kSchema1DomainElement)) {
      Entry domain = ParseDomainOrPath(*domain_node, result);
      if (!domain.text.empty()) {
        std::string prefix = (domain.exclude ? "!" : "");
        domain.list->push_back(prefix + domain.text);
      }
      // Loop over <path> elements.
      for (const base::Value* path_node :
           GetChildrenWithTag(*domain_node, kSchema1PathElement)) {
        Entry path = ParseDomainOrPath(*path_node, result);
        if (!path.text.empty() && !domain.text.empty()) {
          std::string prefix = (path.exclude ? "!" : "");
          path.list->push_back(prefix + domain.text + path.text);
        }
      }
    }
  }
}

// Parses Enterprise Mode schema 2 files according to:
// https://technet.microsoft.com/itpro/internet-explorer/ie11-deploy-guide/enterprise-mode-schema-version-2-guidance
void ParseIeFileVersionTwo(const base::Value& xml, ParsedXml* result) {
  DCHECK(IsXmlElementNamed(xml, kSchema2SiteListElement));
  // Iterate over <site> elements. Notably, skip <created-by> elements.
  for (const base::Value* site_node :
       GetChildrenWithTag(xml, kSchema2SiteElement)) {
    std::string url =
        GetXmlElementAttribute(*site_node, kSchema2SiteUrlAttribute);
    base::TrimWhitespaceASCII(url, base::TRIM_ALL, &url);
    if (url.empty())
      continue;
    // Read all sub-elements and keep the content of the <open-in> element.
    std::string mode;
    for (const base::Value* open_in_node :
         GetChildrenWithTag(*site_node, kSchema2SiteOpenInElement)) {
      GetXmlElementText(*open_in_node, &mode);
    }
    base::TrimWhitespaceASCII(mode, base::TRIM_ALL, &mode);
    std::string prefix = (mode.empty() || mode == "none") ? "!" : "";
    result->sitelist.push_back(prefix + url);
  }
}

void RawXmlParsed(base::OnceCallback<void(ParsedXml)> callback,
                  std::unique_ptr<base::Value> xml,
                  const base::Optional<std::string>& error) {
  if (error) {
    // Copies the string, but it should only be around 20 characters.
    std::move(callback).Run(ParsedXml({}, {}, *error));
    return;
  }
  DCHECK(xml);
  DCHECK(data_decoder::IsXmlElementOfType(
      *xml, data_decoder::mojom::XmlParser::kElementType));
  ParsedXml result;
  if (data_decoder::IsXmlElementNamed(*xml, kSchema1RulesElement)) {
    // Enterprise Mode schema v.1 has <rules> element at its top level.
    ParseIeFileVersionOne(*xml, &result);
  } else if (data_decoder::IsXmlElementNamed(*xml, kSchema2SiteListElement)) {
    // Enterprise Mode schema v.2 has <site-list> element at its top level.
    ParseIeFileVersionTwo(*xml, &result);
  } else {
    result.error = kInvalidRootElement;
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace

ParsedXml::ParsedXml() = default;
ParsedXml::ParsedXml(ParsedXml&&) = default;
ParsedXml::ParsedXml(std::vector<std::string>&& sitelist_,
                     std::vector<std::string>&& greylist_,
                     base::Optional<std::string>&& error_)
    : sitelist(std::move(sitelist_)),
      greylist(std::move(greylist_)),
      error(std::move(error_)) {}
ParsedXml::~ParsedXml() = default;

void ParseIeemXml(const std::string& xml,
                  base::OnceCallback<void(ParsedXml)> callback) {
  data_decoder::ParseXml(
      content::ServiceManagerConnection::GetForProcess()->GetConnector(), xml,
      base::BindOnce(&RawXmlParsed, std::move(callback)));
}

}  // namespace browser_switcher
