// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {Cdd} from './cdd.js';
import {Destination, DestinationCertificateStatus, DestinationConnectionStatus, DestinationOptionalParams, DestinationOrigin, DestinationType} from './destination.js';

/**
 * Enumeration of cloud destination field names.
 */
enum CloudDestinationField {
  CAPABILITIES = 'capabilities',
  CONNECTION_STATUS = 'connectionStatus',
  DESCRIPTION = 'description',
  DISPLAY_NAME = 'displayName',
  ID = 'id',
  LAST_ACCESS = 'accessTime',
  TAGS = 'tags',
  TYPE = 'type'
}

/**
 * Special tag that denotes whether the destination is owned by the user.
 */
const OWNED_TAG: string = '^own';

/**
 * Tag that denotes whether the printer passes the 2018 certificate.
 */
const CERT_TAG: string = '__cp_printer_passes_2018_cert__=';

/**
 * Enumeration of cloud destination types that are supported by print preview.
 */
enum DestinationCloudType {
  ANDROID = 'ANDROID_CHROME_SNAPSHOT',
  DOCS = 'DOCS',
  IOS = 'IOS_CHROME_SNAPSHOT'
}

/**
 * Returns the destination type.
 * @param typeStr Destination type given by the Google Cloud Print server.
 */
function parseType(typeStr: string): DestinationType {
  if (typeStr === DestinationCloudType.ANDROID ||
      typeStr === DestinationCloudType.IOS) {
    return DestinationType.MOBILE;
  }
  if (typeStr === DestinationCloudType.DOCS) {
    return DestinationType.GOOGLE_PROMOTED;
  }
  return DestinationType.GOOGLE;
}

/**
 * @param tags The array of tag strings sent by GCP server.
 * @return The certificate status indicated by the tag. Returns NONE if
 *     certificate tag is not found.
 */
function extractCertificateStatus(tags: string[]):
    DestinationCertificateStatus {
  const certTag = tags.find(tag => tag.startsWith(CERT_TAG));
  if (!certTag) {
    return DestinationCertificateStatus.NONE;
  }
  const value =
      certTag.substring(CERT_TAG.length) as DestinationCertificateStatus;
  // Only 2 valid values sent by GCP server.
  assert(
      value === DestinationCertificateStatus.UNKNOWN ||
      value === DestinationCertificateStatus.YES ||
      value === DestinationCertificateStatus.NO);
  return value;
}

export type CloudDestinationInfo = {
  [field in CloudDestinationField]: (string|string[]|Cdd|undefined)
};

/**
 * Parses a destination from JSON from a Google Cloud Print search or printer
 * response.
 * @param json Object that represents a Google Cloud Print search or printer
 *     response.
 * @param origin The origin of the response.
 * @param account The account this destination is registered for or empty
 *     string, if origin !== COOKIES.
 * @return Parsed destination.
 */
export function parseCloudDestination(
    json: CloudDestinationInfo, origin: DestinationOrigin,
    account: string): Destination {
  if (!json.hasOwnProperty(CloudDestinationField.ID) ||
      !json.hasOwnProperty(CloudDestinationField.TYPE) ||
      !json.hasOwnProperty(CloudDestinationField.DISPLAY_NAME)) {
    throw Error('Cloud destination does not have an ID or a display name');
  }
  const id: string = json[CloudDestinationField.ID] as string;
  const tags: string[] = (json[CloudDestinationField.TAGS] as string[]) || [];
  const connectionStatus: DestinationConnectionStatus =
      json[CloudDestinationField.CONNECTION_STATUS] as
          DestinationConnectionStatus ||
      DestinationConnectionStatus.UNKNOWN;
  const optionalParams: DestinationOptionalParams = {
    account: account,
    tags: tags,
    isOwned: tags.includes(OWNED_TAG),
    lastAccessTime:
        parseInt((json[CloudDestinationField.LAST_ACCESS] as string), 10) ||
        Date.now(),
    cloudID: id,
    description: json[CloudDestinationField.DESCRIPTION] as string,
    certificateStatus: extractCertificateStatus(tags),
  };
  const cloudDest = new Destination(
      id, parseType(json[CloudDestinationField.TYPE] as string), origin,
      json[CloudDestinationField.DISPLAY_NAME] as string, connectionStatus,
      optionalParams);
  if (json.hasOwnProperty(CloudDestinationField.CAPABILITIES)) {
    cloudDest.capabilities = json[CloudDestinationField.CAPABILITIES] as Cdd;
  }
  return cloudDest;
}
