// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_MEDIA_PLAYER_API_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_MEDIA_PLAYER_API_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class MediaPlayerEventRouter;

class MediaPlayerAPI : public BrowserContextKeyedAPI {
 public:
  explicit MediaPlayerAPI(content::BrowserContext* context);

  MediaPlayerAPI(const MediaPlayerAPI&) = delete;
  MediaPlayerAPI& operator=(const MediaPlayerAPI&) = delete;

  ~MediaPlayerAPI() override;

  // Convenience method to get the MediaPlayerAPI for a profile.
  static MediaPlayerAPI* Get(content::BrowserContext* context);

  MediaPlayerEventRouter* media_player_event_router();

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<MediaPlayerAPI>* GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<MediaPlayerAPI>;

  const raw_ptr<content::BrowserContext> browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "MediaPlayerAPI";
  }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  std::unique_ptr<MediaPlayerEventRouter> media_player_event_router_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_MEDIA_PLAYER_API_H_
