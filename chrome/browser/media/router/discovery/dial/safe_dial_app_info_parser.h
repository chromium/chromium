// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_SAFE_DIAL_APP_INFO_PARSER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_SAFE_DIAL_APP_INFO_PARSER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media/router/discovery/dial/parsed_dial_app_info.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace media_router {

// SafeDialAppInfoParser parses the given app info XML file safely via a utility
// process.
// Spec for DIAL app info XML:
// http://www.dial-multiscreen.org/dial-protocol-specification
// Section 6.1.2 Server response.
class SafeDialAppInfoParser {
 public:
  enum ParsingResult {
    kSuccess = 0,
    kInvalidXML = 1,
    kFailToReadName = 2,
    kFailToReadState = 3,
    kMissingName = 4,
    kInvalidState = 5
  };

  SafeDialAppInfoParser();

  SafeDialAppInfoParser(const SafeDialAppInfoParser&) = delete;
  SafeDialAppInfoParser& operator=(const SafeDialAppInfoParser&) = delete;

  virtual ~SafeDialAppInfoParser();

  // Callback function invoked when done parsing DIAL app info XML.
  // |app_info|: app info object. Empty if parsing failed.
  // |parsing_error|: error encountered while parsing the DIAL app info XML.
  using ParseCallback =
      base::OnceCallback<void(std::unique_ptr<ParsedDialAppInfo> app_info,
                              ParsingResult parsing_result)>;

  // Parses the DIAL app info in |xml_text| in a utility process.
  // If the parsing succeeds, invokes callback with a valid
  // |app_info|, otherwise invokes callback with an empty
  // |app_info| and sets parsing error to detail the failure.
  // Note that it's safe to call this method multiple times and when making
  // multiple calls they may be grouped in the same utility process. The
  // utility process is still cleaned up automatically if unused after some
  // time, even if this object is still alive.
  // Note also that the callback is not called if the object is deleted.
  virtual void Parse(const std::string& xml_text, ParseCallback callback);

 private:
  void OnXmlParsingDone(ParseCallback callback,
                        data_decoder::DataDecoder::ValueOrError result);

  base::WeakPtrFactory<SafeDialAppInfoParser> weak_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_SAFE_DIAL_APP_INFO_PARSER_H_
