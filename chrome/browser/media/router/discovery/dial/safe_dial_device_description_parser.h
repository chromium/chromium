// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_SAFE_DIAL_DEVICE_DESCRIPTION_PARSER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_SAFE_DIAL_DEVICE_DESCRIPTION_PARSER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media/router/discovery/dial/parsed_dial_device_description.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

class GURL;

namespace media_router {

// SafeDialDeviceDescriptionParser parses the given device description XML file
// safely via a utility process.
// Spec for DIAL device description:
// http://upnp.org/specs/arch/UPnP-arch-DeviceArchitecture-v2.0.pdf
// Section 2.3 Device description.
class SafeDialDeviceDescriptionParser {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. This class must stay in sync with
  // the UMA enum MediaRouterDeviceDescriptionParsingResult.
  enum class ParsingResult {
    kSuccess = 0,
    kInvalidXml,
    kFailedToReadUdn,
    kFailedToReadFriendlyName,
    kFailedToReadModelName,
    kFailedToReadDeviceType,
    kMissingUniqueId,
    kMissingFriendlyName,
    kMissingAppUrl,
    kInvalidAppUrl,
    kUtilityProcessError,
    kMaxValue = kUtilityProcessError,
  };

  SafeDialDeviceDescriptionParser();

  SafeDialDeviceDescriptionParser(const SafeDialDeviceDescriptionParser&) =
      delete;
  SafeDialDeviceDescriptionParser& operator=(
      const SafeDialDeviceDescriptionParser&) = delete;

  ~SafeDialDeviceDescriptionParser();

  // Callback function invoked when done parsing some device description XML.
  // |device_description|: device description object. Empty if parsing failed.
  // |parsing_error|: error encountered while parsing the DIAL device
  // description.
  using ParseCallback = base::OnceCallback<void(
      const ParsedDialDeviceDescription& device_description,
      ParsingResult parsing_result)>;

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
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_SAFE_DIAL_DEVICE_DESCRIPTION_PARSER_H_
