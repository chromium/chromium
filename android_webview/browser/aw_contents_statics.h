// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_STATICS_H_
#define ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_STATICS_H_

#include "net/socket/socket_tag.h"

namespace android_webview {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This must be kept in sync with the
// public API in androidx.webkit.WebViewCompat.
enum class RendererLibraryPrefetchMode {
  kDefault = 0,
  kDisabled = 1,
  kEnabled = 2,
  kMaxValue = kEnabled,
};

// Retrieves the DefaultTrafficStatsTag value set by the embedder.
net::SocketTag GetDefaultSocketTag();

void SetRendererLibraryPrefetchMode(RendererLibraryPrefetchMode mode);
RendererLibraryPrefetchMode GetRendererLibraryPrefetchMode();

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_STATICS_H_
