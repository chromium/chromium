// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CHROME_CAST_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CHROME_CAST_MESSAGE_HANDLER_H_

namespace cast_channel {
class CastMessageHandler;
}

namespace media_router {

// Returns the singleton instance of CastMessageHandler with Chrome as the
// embedder, creating one if necessary. This must be invoked on the UI thread.
cast_channel::CastMessageHandler* GetCastMessageHandler();

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CHROME_CAST_MESSAGE_HANDLER_H_
