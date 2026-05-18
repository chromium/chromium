// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_STATICS_H_
#define ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_STATICS_H_

#include "net/socket/socket_tag.h"

namespace android_webview {

// Retrieves the DefaultTrafficStatsTag value set by the embedder.
net::SocketTag GetDefaultSocketTag();

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_STATICS_H_
