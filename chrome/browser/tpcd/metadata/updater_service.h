// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_METADATA_UPDATER_SERVICE_H_
#define CHROME_BROWSER_TPCD_METADATA_UPDATER_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/tpcd/metadata/parser.h"

namespace content {
class BrowserContext;
}

namespace content_settings {
class CookieSettings;
}

namespace tpcd::metadata {
class UpdaterService : public KeyedService, public Parser::Observer {
 public:
  explicit UpdaterService(content::BrowserContext* context);
  ~UpdaterService() override;
  UpdaterService(const UpdaterService&) = delete;
  UpdaterService& operator=(const UpdaterService&) = delete;

 private:
  // KeyedService Start:
  void Shutdown() override;
  // KeyedService End.

  // Parser::Observer Start:
  void OnMetadataReady() override;
  // Parser::Observer End.

  raw_ptr<content::BrowserContext> browser_context_;
  raw_ptr<Parser> parser_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  base::WeakPtrFactory<UpdaterService> weak_ptr_factory_{this};
};

}  // namespace tpcd::metadata

#endif  // CHROME_BROWSER_TPCD_METADATA_UPDATER_SERVICE_H_
