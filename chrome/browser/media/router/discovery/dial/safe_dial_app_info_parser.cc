// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/safe_dial_app_info_parser.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/data_decoder_util.h"
#include "services/data_decoder/public/cpp/safe_xml_parser.h"
#include "url/gurl.h"

namespace media_router {

namespace {

DialAppState ParseDialAppState(const std::string& app_state) {
  if (base::ToLowerASCII(app_state) == "running") {
    return DialAppState::kRunning;
  } else if (base::ToLowerASCII(app_state) == "stopped") {
    return DialAppState::kStopped;
  }
  return DialAppState::kUnknown;
}

// Parses |child_element| content, and sets corresponding fields of
// |out_app_info|. Returns ParsingResult::kSuccess if parsing succeeds.
SafeDialAppInfoParser::ParsingResult ProcessChildElement(
    const base::Value& child_element,
    ParsedDialAppInfo* out_app_info) {
  std::string tag_name;
  if (!data_decoder::GetXmlElementTagName(child_element, &tag_name))
    return SafeDialAppInfoParser::ParsingResult::kInvalidXML;

  if (tag_name == "name") {
    if (!data_decoder::GetXmlElementText(child_element, &out_app_info->name))
      return SafeDialAppInfoParser::ParsingResult::kFailToReadName;
  } else if (tag_name == "options") {
    out_app_info->allow_stop = data_decoder::GetXmlElementAttribute(
                                   child_element, "allowStop") != "false";
  } else if (tag_name == "link") {
    out_app_info->href =
        data_decoder::GetXmlElementAttribute(child_element, "href");
  } else if (tag_name == "state") {
    std::string state;
    if (!data_decoder::GetXmlElementText(child_element, &state))
      return SafeDialAppInfoParser::ParsingResult::kFailToReadState;
    out_app_info->state = ParseDialAppState(state);
  } else {
    std::string extra_data;
    if (!data_decoder::GetXmlElementText(child_element, &extra_data)) {
      DVLOG(2) << "Fail to read extra data, <" << tag_name << ">"
               << " is not a plain text element.";
    } else {
      out_app_info->extra_data[tag_name] = extra_data;
    }
  }

  return SafeDialAppInfoParser::ParsingResult::kSuccess;
}

// Returns ParsingResult::kSuccess if mandatory fields (name, state) are valid.
// |app_info|: app info object to be validated.
SafeDialAppInfoParser::ParsingResult ValidateParsedAppInfo(
    const ParsedDialAppInfo& app_info) {
  if (app_info.name.empty())
    return SafeDialAppInfoParser::ParsingResult::kMissingName;

  if (app_info.state == DialAppState::kUnknown)
    return SafeDialAppInfoParser::ParsingResult::kInvalidState;

  return SafeDialAppInfoParser::ParsingResult::kSuccess;
}

}  // namespace

SafeDialAppInfoParser::SafeDialAppInfoParser() = default;

SafeDialAppInfoParser::~SafeDialAppInfoParser() {}

void SafeDialAppInfoParser::Parse(const std::string& xml_text,
                                  ParseCallback callback) {
  DVLOG(2) << "Parsing app info...";
  DCHECK(callback);

  GetDataDecoder().ParseXml(
      xml_text,
      base::BindOnce(&SafeDialAppInfoParser::OnXmlParsingDone,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void SafeDialAppInfoParser::OnXmlParsingDone(
    SafeDialAppInfoParser::ParseCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    DVLOG(1) << "Fail to parse XML in utility process, error: "
             << *result.error;
  }

  if (!result.value || !result.value->is_dict()) {
    std::move(callback).Run(nullptr, ParsingResult::kInvalidXML);
    return;
  }

  // NOTE: enforce namespace check for <service> element in future. Namespace
  // value will be "urn:dial-multiscreen-org:schemas:dial".
  bool unique_service = true;
  const base::Value* service_element = data_decoder::FindXmlElementPath(
      *result.value, {"service"}, &unique_service);
  if (!service_element || !unique_service) {
    std::move(callback).Run(nullptr, ParsingResult::kInvalidXML);
    return;
  }

  // Read optional @dialVer.
  std::unique_ptr<ParsedDialAppInfo> app_info =
      std::make_unique<ParsedDialAppInfo>();
  app_info->dial_version =
      data_decoder::GetXmlElementAttribute(*service_element, "dialVer");

  // Fetch all the children of <service> element.
  const base::Value* child_elements =
      data_decoder::GetXmlElementChildren(*service_element);
  if (!child_elements || !child_elements->is_list()) {
    std::move(callback).Run(nullptr, ParsingResult::kInvalidXML);
    return;
  }

  ParsingResult parsing_result = ParsingResult::kSuccess;
  for (const auto& child_element : child_elements->GetList()) {
    parsing_result = ProcessChildElement(child_element, app_info.get());
    if (parsing_result != ParsingResult::kSuccess) {
      std::move(callback).Run(nullptr, parsing_result);
      return;
    }
  }

  // Validate mandatory fields (name, state).
  parsing_result = ValidateParsedAppInfo(*app_info);
  if (parsing_result != ParsingResult::kSuccess) {
    std::move(callback).Run(nullptr, parsing_result);
    return;
  }

  std::move(callback).Run(std::move(app_info), ParsingResult::kSuccess);
}

}  // namespace media_router
