// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_CONSTANTS_H_
#define CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_CONSTANTS_H_

const char kInstantMessagingReceiveMessageAPI[] =
    "https://instantmessaging-pa.googleapis.com/v1/messages:receiveExpress";

const char kInstantMessagingSendMessageAPI[] =
    "https://instantmessaging-pa.googleapis.com/v1/message:sendExpress";

// Template for optional OAuth2 authorization HTTP header.
const char kAuthorizationHeaderFormat[] = "Authorization: Bearer %s";

#endif  // CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_CONSTANTS_H_
