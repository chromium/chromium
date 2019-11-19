// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_SAFE_DIAL_DEVICE_DESCRIPTION_PARSER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_SAFE_DIAL_DEVICE_DESCRIPTION_PARSER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/values.h"
#include "chrome/browser/media/router/discovery/dial/parsed_dial_device_description.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

class GURL;

namespace media_router {

class DataDecoder;

// SafeDialDeviceDescriptionParser parses the given device description XML file
// safely via a utility process.
// Spec for DIAL device description:
// http://upnp.org/specs/arch/UPnP-arch-DeviceArchitecture-v2.0.pdf
// Section 2.3 Device description.
class SafeDialDeviceDescriptionParser {
 public:
  enum class ParsingError : int32_t {
    kNone = 0,
    kInvalidXml = 1,
    kFailedToReadUdn = 2,
    kFailedToReadFriendlyName = 3,
    kFailedToReadModelName = 4,
    kFailedToReadDeviceType = 5,
    kMissingUniqueId = 6,
    kMissingFriendlyName = 7,
    kMissingAppUrl = 8,
    kInvalidAppUrl = 9,
    kUtilityProcessError = 10,

    // Note: Add entries only immediately above this line.
    // TODO(https://crbug.com/742517): remove this enum value.
    kTotalCount = 11,
  };

  SafeDialDeviceDescriptionParser();
  ~SafeDialDeviceDescriptionParser();

  // Callback function invoked when done parsing some device description XML.
  // |device_description|: device description object. Empty if parsing failed.
  // |parsing_error|: error encountered while parsing the DIAL device
  // description.
  using ParseCallback = base::OnceCallback<void(
      const ParsedDialDeviceDescription& device_description,
      ParsingError parsing_error)>;

  // Parses the device description in |xml_text| in a utility process.
  // If the parsing succeeds, invokes callback with a valid
  // |device_description|, otherwise invokes callback with an empty
  // |device_description| and sets parsing error to detail the failure.
  // |app_url| is the app URL that should be set on the
  // ParsedDialDeviceDescription object passed in the callback.
  // Note that it's safe to call this method multiple times and when making
  // multiple calls they may be grouped in the same utility process. The
  // utility process is still cleaned up automatically if unused after some
  // time, even if this object is still alive.
  // Note also that the callback is not called if the object is deleted.
  void Parse(const std::string& xml_text,
             const GURL& app_url,
             ParseCallback callback);

 private:
  void OnXmlParsingDone(SafeDialDeviceDescriptionParser::ParseCallback callback,
                        const GURL& app_url,
                        data_decoder::DataDecoder::ValueOrError result);

  base::WeakPtrFactory<SafeDialDeviceDescriptionParser> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SafeDialDeviceDescriptionParser);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_SAFE_DIAL_DEVICE_DESCRIPTION_PARSER_H_
