// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/ieem_sitelist_parser.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_switcher/browser_switcher_features.h"
#include "content/public/browser/browser_thread.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/safe_xml_parser.h"
#include "services/data_decoder/public/mojom/xml_parser.mojom.h"

namespace browser_switcher {

namespace {

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
  data_decoder::GetAllXmlElementChildrenWithTag(node, tag, &children);
  return children;
}

// Data in a v.1 schema <domain> or <path> element.
struct Entry {
  // URL or path concerned.
  std::string text;
  // True if the exclude attribute is "true".
  bool exclude;
  // True if the doNotTransition attribute is "true".
  bool do_not_transition;
};

Entry ParseDomainOrPath(const base::Value& node, ParsedXml* result) {
  DCHECK(data_decoder::IsXmlElementNamed(node, kSchema1DomainElement) ||
         data_decoder::IsXmlElementNamed(node, kSchema1PathElement));

  Entry entry;

  std::string exclude_attrib =
      data_decoder::GetXmlElementAttribute(node, kSchema1ExcludeAttribute);
  entry.exclude = (exclude_attrib == kSchema1TrueValue);

  std::string do_not_transition_attrib = data_decoder::GetXmlElementAttribute(
      node, kSchema1DoNotTransitionAttribute);
  entry.do_not_transition = (do_not_transition_attrib == kSchema1TrueValue);

  data_decoder::GetXmlElementText(node, &entry.text);
  base::TrimWhitespaceASCII(entry.text, base::TRIM_ALL, &entry.text);

  return entry;
}

// Parses Enterprise Mode schema 1 files according to:
// https://technet.microsoft.com/itpro/internet-explorer/ie11-deploy-guide/enterprise-mode-schema-version-1-guidance
void ParseIeFileVersionOne(const base::Value& xml,
                           ParsingMode parsing_mode,
                           ParsedXml* result) {
  const bool none_is_greylist =
      parsing_mode == ParsingMode::kIESiteListMode &&
      base::FeatureList::IsEnabled(kBrowserSwitcherNoneIsGreylist);

  DCHECK(data_decoder::IsXmlElementNamed(xml, kSchema1RulesElement));
  for (const base::Value& node : *data_decoder::GetXmlElementChildren(xml)) {
    // Skip over anything that is not a <emie> or <docMode> element.
    if (!data_decoder::IsXmlElementNamed(node, kSchema1EmieElement) &&
        !data_decoder::IsXmlElementNamed(node, kSchema1DocModeElement)) {
      continue;
    }
    // Loop over <domain> elements.
    for (const base::Value* domain_node :
         GetChildrenWithTag(node, kSchema1DomainElement)) {
      Entry domain = ParseDomainOrPath(*domain_node, result);
      if (!domain.text.empty() && !domain.exclude) {
        if (none_is_greylist) {
          if (domain.do_not_transition) {
            // doNotTransition="true" means greylist.
            result->rules.greylist.push_back(domain.text);
          } else {
            // doNotTransition="false", absent or unrecognized means sitelist.
            result->rules.sitelist.push_back(domain.text);
          }
        } else {
          // TODO(crbug.com/40812726): Remove this else branch, and the
          // kBrowserSwitcherNoneIsGreylist flag, once we're confident this
          // doesn't break customers. This was added in M99.
          std::string prefix = (domain.do_not_transition ? "!" : "");
          result->rules.sitelist.push_back(prefix + domain.text);
        }
      }
      // Loop over <path> elements.
      for (const base::Value* path_node :
           GetChildrenWithTag(*domain_node, kSchema1PathElement)) {
        Entry path = ParseDomainOrPath(*path_node, result);
        if (!path.text.empty() && !domain.text.empty() && !path.exclude) {
          if (none_is_greylist) {
            if (path.do_not_transition) {
              // doNotTransition="true" means greylist.
              result->rules.greylist.push_back(domain.text + path.text);
            } else {
              // doNotTransition="false", absent or unrecognized means sitelist.
              result->rules.sitelist.push_back(domain.text + path.text);
            }
          } else {
            // TODO(crbug.com/40812726): Remove this else branch, and the
            // kBrowserSwitcherNoneIsGreylist flag, once we're confident this
            // doesn't break customers. This was added in M99.
            std::string prefix = (path.do_not_transition ? "!" : "");
            result->rules.sitelist.push_back(prefix + domain.text + path.text);
          }
        }
      }
    }
  }
}

// Parses Enterprise Mode schema 2 files according to:
// https://technet.microsoft.com/itpro/internet-explorer/ie11-deploy-guide/enterprise-mode-schema-version-2-guidance
void ParseIeFileVersionTwo(const base::Value& xml,
                           ParsingMode parsing_mode,
                           ParsedXml* result) {
  const bool none_is_greylist =
      parsing_mode == ParsingMode::kIESiteListMode &&
      base::FeatureList::IsEnabled(kBrowserSwitcherNoneIsGreylist);

  DCHECK(data_decoder::IsXmlElementNamed(xml, kSchema2SiteListElement));
  // Iterate over <site> elements. Notably, skip <created-by> elements.
  for (const base::Value* site_node :
       GetChildrenWithTag(xml, kSchema2SiteElement)) {
    std::string url = data_decoder::GetXmlElementAttribute(
        *site_node, kSchema2SiteUrlAttribute);
    base::TrimWhitespaceASCII(url, base::TRIM_ALL, &url);
    if (url.empty())
      continue;
    // Read all sub-elements and keep the content of the <open-in> element.
    std::string open_in;
    for (const base::Value* open_in_node :
         GetChildrenWithTag(*site_node, kSchema2SiteOpenInElement)) {
      data_decoder::GetXmlElementText(*open_in_node, &open_in);
    }
    base::TrimWhitespaceASCII(open_in, base::TRIM_ALL, &open_in);

    if (none_is_greylist) {
      if (!base::CompareCaseInsensitiveASCII(open_in, "ie11")) {
        // <open-in>IE11 means sitelist.
        result->rules.sitelist.push_back(url);
      } else if (!base::CompareCaseInsensitiveASCII(open_in, "msedge") ||
                 !base::CompareCaseInsensitiveASCII(open_in, "chrome")) {
        // <open-in>MSEdge or <open-in>Chrome means an inverted rule (i.e., open
        // in Chrome).
        result->rules.sitelist.push_back("!" + url);
      } else {
        // <open-in> absent, unrecognized, or "none" means greylist.
        result->rules.greylist.push_back(url);
      }
    } else {
      // TODO(crbug.com/40812726): Remove this else branch, and the
      // kBrowserSwitcherNoneIsGreylist flag, once we're confident this
      // doesn't break customers. This was added in M99.
      std::string prefix = (open_in.empty() ||
                            !base::CompareCaseInsensitiveASCII(open_in, "none"))
                               ? "!"
                               : "";
      result->rules.sitelist.push_back(prefix + url);
    }
  }
}

void RawXmlParsed(ParsingMode parsing_mode,
                  base::OnceCallback<void(ParsedXml)> callback,
                  data_decoder::DataDecoder::ValueOrError xml) {
  if (!xml.has_value()) {
    // Copies the string, but it should only be around 20 characters.
    std::move(callback).Run(ParsedXml({}, {}, xml.error()));
    return;
  }
  DCHECK(data_decoder::IsXmlElementOfType(
      *xml, data_decoder::mojom::XmlParser::kElementType));
  ParsedXml result;
  if (data_decoder::IsXmlElementNamed(*xml, kSchema1RulesElement)) {
    // Enterprise Mode schema v.1 has <rules> element at its top level.
    ParseIeFileVersionOne(*xml, parsing_mode, &result);
  } else if (data_decoder::IsXmlElementNamed(*xml, kSchema2SiteListElement)) {
    // Enterprise Mode schema v.2 has <site-list> element at its top level.
    ParseIeFileVersionTwo(*xml, parsing_mode, &result);
  } else {
    result.error = kInvalidRootElement;
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace

ParsedXml::ParsedXml() = default;
ParsedXml::ParsedXml(ParsedXml&&) = default;
ParsedXml::ParsedXml(RawRuleSet&& rules_, std::optional<std::string>&& error_)
    : rules(std::move(rules_)), error(std::move(error_)) {}
ParsedXml::ParsedXml(std::vector<std::string>&& sitelist,
                     std::vector<std::string>&& greylist,
                     std::optional<std::string>&& error)
    : ParsedXml(RawRuleSet(std::move(sitelist), std::move(greylist)),
                std::move(error)) {}
ParsedXml::~ParsedXml() = default;

void ParseIeemXml(const std::string& xml,
                  ParsingMode parsing_mode,
                  base::OnceCallback<void(ParsedXml)> callback) {
  data_decoder::DataDecoder::ParseXmlIsolated(
      xml, data_decoder::mojom::XmlParser::WhitespaceBehavior::kIgnore,
      base::BindOnce(&RawXmlParsed, parsing_mode, std::move(callback)));
}

}  // namespace browser_switcher
