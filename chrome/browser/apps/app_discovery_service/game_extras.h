// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_GAME_EXTRAS_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_GAME_EXTRAS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/apps/app_discovery_service/result.h"
#include "url/gurl.h"

namespace apps {

class GameExtras : public SourceExtras {
 public:
  GameExtras(const std::u16string& source,
             const base::FilePath& relative_icon_path,
             const bool is_icon_masking_allowed,
             const GURL& deeplink_url);
  GameExtras(const GameExtras&);
  GameExtras& operator=(const GameExtras&) = delete;
  ~GameExtras() override;

  // Result::SourceExtras:
  std::unique_ptr<SourceExtras> Clone() override;
  GameExtras* AsGameExtras() override;

  // The source from which the game is being pulled from.
  const std::u16string& GetSource() const;
  const base::FilePath& GetRelativeIconPath() const;
  bool GetIsIconMaskingAllowed() const;
  const GURL& GetDeeplinkUrl() const;

 private:
  std::u16string source_;
  base::FilePath relative_icon_path_;
  bool is_icon_masking_allowed_;
  GURL deeplink_url_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_GAME_EXTRAS_H_
