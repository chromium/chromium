// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_APP_ARC_PLAYSTORE_SEARCH_REQUEST_STATE_H_
#define ASH_COMPONENTS_ARC_APP_ARC_PLAYSTORE_SEARCH_REQUEST_STATE_H_

namespace arc {

enum class ArcPlayStoreSearchRequestState {
  // Request handled successfully.
  SUCCESS = 0,
  // Request canceled when a newer request is sent.
  CANCELED = 1,
  // Request failed due to any communication error or Play Store internal error.
  ERROR_DEPRECATED = 2,

  // All possible reasons of ending a request:
  //   PlayStoreProxyService is not available.
  PLAY_STORE_PROXY_NOT_AVAILABLE = 3,
  //   It fails to cancel the previous request.
  FAILED_TO_CALL_CANCEL = 4,
  //   It fails to call findApps API.
  FAILED_TO_CALL_FINDAPPS = 5,
  //   It comes with invalid parameters.
  REQUEST_HAS_INVALID_PARAMS = 6,
  //   It times out.
  REQUEST_TIMEOUT = 7,
  //   At least one result returned from Phonesky has an unmatched request code.
  PHONESKY_RESULT_REQUEST_CODE_UNMATCHED = 8,
  //   At least one result returned from Phonesky has an unmatched session id.
  PHONESKY_RESULT_SESSION_ID_UNMATCHED = 9,
  //   Phonesky returns with an unmatched request code.
  PHONESKY_REQUEST_REQUEST_CODE_UNMATCHED = 10,
  //   The app discovery service is not available.
  PHONESKY_APP_DISCOVERY_NOT_AVAILABLE = 11,
  //   The installed Phonesky version doesn't support app discovery.
  PHONESKY_VERSION_NOT_SUPPORTED = 12,
  //   It gets an unexpected exception from Phonesky.
  PHONESKY_UNEXPECTED_EXCEPTION = 13,
  //   The Phonesky app discovery service thinks it's malformed.
  PHONESKY_MALFORMED_QUERY = 14,
  //   An internal error happens in Phonesky while processing the request.
  PHONESKY_INTERNAL_ERROR = 15,
  //   At least one result returned with invalid app data.
  PHONESKY_RESULT_INVALID_DATA = 16,
  //   An invalid result arrives at Chrome, which may be caused by mojo
  //   versions mismatch between Android And Chrome.
  CHROME_GOT_INVALID_RESULT = 17,

  // Total amount of states. New states should be added before this.
  STATE_COUNT,
};
}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_APP_ARC_PLAYSTORE_SEARCH_REQUEST_STATE_H_
